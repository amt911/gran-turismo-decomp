// Unit test for gt::fiber + gt::call_guest: a decompiled function whose guest callee blocks is
// suspended, another "thread" runs, and the function finishes once its thread comes back. The
// scheduler is simulated by hand-registered guest functions. Plain executable, run by CTest.
#include "fiber.hpp"
#include "guest_call.hpp"

#include <cstdio>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

constexpr std::uint32_t kBlocking = 0x08900400u;   // blocks: switches to thread B
constexpr std::uint32_t kAfterWake = 0x08900500u;  // rest of the blocking call, back on thread A
constexpr std::uint32_t kThreadB = 0x08900600u;    // thread B: runs, then switches back to A
constexpr std::uint32_t kEntry = 0x08900700u;      // the decompiled function's address
constexpr std::uint32_t kCallerReturn = 0x08A00000u;

psprecomp::AllegrexContext g_thread_a{};
int g_b_ran = 0;

void blocking(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    g_thread_a = ctx;
    g_thread_a.pc = kAfterWake;
    psprecomp::set_runtime_thread_identity(2, "B");
    ctx = psprecomp::AllegrexContext{};
    ctx.pc = kThreadB;
}

void after_wake(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 7u;
    ctx.pc = ctx.gpr[31];
}

void thread_b(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ++g_b_ran;
    psprecomp::set_runtime_thread_identity(1, "A");
    ctx = g_thread_a;
}

// The decompiled function: v0 = blocking() + 1.
void decompiled(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = gt::call_guest(rt, ctx, kBlocking, {}) + 1u;
}

} // namespace

int main() {
    psprecomp::Runtime rt;
    rt.register_function(kBlocking, &blocking, "test_blocking");
    rt.register_function(kAfterWake, &after_wake, "test_after_wake");
    rt.register_function(kThreadB, &thread_b, "test_thread_b");
    psprecomp::set_runtime_thread_identity(1, "A");

    psprecomp::AllegrexContext ctx{};
    ctx.gpr[29] = 0x09F00000u;
    ctx.gpr[31] = kCallerReturn;
    gt::fiber::run(rt, ctx, &decompiled, kEntry);
    check(ctx.pc == kThreadB, "after the block the dispatcher continues with thread B");
    check(!gt::fiber::on_fiber(), "the suspended fiber is no longer current");

    for (int step = 0; step < 16 && ctx.pc != kCallerReturn && !rt.stopped(); ++step) {
        const auto function = rt.function_at(ctx.pc);
        if (function == nullptr) break;
        function(rt, ctx);
    }
    check(g_b_ran == 1, "thread B ran while thread A was blocked");
    check(ctx.pc == kCallerReturn, "thread A finally returns to the decompiled function's caller");
    check(ctx.gpr[2] == 8u, "the decompiled function resumed after the block and finished");
    check(ctx.gpr[29] == 0x09F00000u, "thread A's sp is back");
    check(!rt.stopped(), "no stop");

    // A second call reuses the finished fiber and runs straight through.
    psprecomp::AllegrexContext again{};
    again.gpr[31] = kCallerReturn;
    gt::fiber::run(rt, again, [](psprecomp::Runtime &, psprecomp::AllegrexContext &c) { c.gpr[2] = 5u; }, kEntry);
    check(again.gpr[2] == 5u && again.pc == kCallerReturn, "a non-blocking call returns directly");
    if (failures == 0) std::puts("fiber: all tests passed");
    return failures == 0 ? 0 : 1;
}
