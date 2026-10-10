// Unit tests for gt::call_guest (host/guest_call.cpp) on a bare Runtime with hand-registered guest
// functions. Plain executable, run by CTest.
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

constexpr std::uint32_t kAdd = 0x08900000u;
constexpr std::uint32_t kDoubleThenAdd = 0x08900100u;
constexpr std::uint32_t kSwitchThread = 0x08900200u;
constexpr std::uint32_t kScale = 0x08900300u;

void add(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {  // v0 = a0 + a1; return
    ctx.gpr[2] = ctx.gpr[4] + ctx.gpr[5];
    ctx.gpr[8] = 0xDEADu;  // clobbers a temporary
    ctx.pc = ctx.gpr[31];
}

void double_then_add(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {  // tail call: add(2*a0, a1)
    ctx.gpr[4] *= 2u;
    ctx.pc = kAdd;
}

void scale(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {  // f0 = f12 * f13 + a0
    ctx.fpr[0] = ctx.fpr[12] * ctx.fpr[13] + static_cast<float>(ctx.gpr[4]);
    ctx.pc = ctx.gpr[31];
}

void switch_thread(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    psprecomp::set_runtime_thread_identity(7, "other");
    ctx.pc = ctx.gpr[31];
}

} // namespace

int main() {
    psprecomp::Runtime rt;
    rt.register_function(kAdd, &add, "test_add");
    rt.register_function(kDoubleThenAdd, &double_then_add, "test_double_then_add");
    rt.register_function(kSwitchThread, &switch_thread, "test_switch_thread");
    rt.register_function(kScale, &scale, "test_scale");
    psprecomp::AllegrexContext caller{};
    caller.gpr[29] = 0x09F00000u;
    caller.gpr[8] = 0x1234u;
    caller.gpr[31] = 0x08A00000u;

    check(gt::call_guest(rt, caller, kAdd, {40u, 2u}) == 42u, "arguments in a0/a1, result from v0");
    check(caller.gpr[8] == 0x1234u && caller.gpr[31] == 0x08A00000u, "the caller's context is untouched");
    check(gt::call_guest(rt, caller, kDoubleThenAdd, {20u, 2u}) == 42u, "runs through tail calls to the return");
    check(gt::call_guest(rt, caller, kScale, {1u}, {2.5f, 4.0f}).f0 == 11.0f, "float arguments in $f12.., result in $f0");
    check(!rt.stopped(), "normal calls don't stop the runtime");

    check(gt::call_guest(rt, caller, kSwitchThread, {}) == 0u, "a thread switch returns 0");
    check(rt.stopped() && rt.stop_reason().find("switched PSP threads") != std::string::npos,
          "a thread switch stops the runtime with the reason");

    psprecomp::Runtime empty;
    check(gt::call_guest(empty, caller, 0x08900300u, {}) == 0u && empty.stopped(), "a missing function stops");
    if (failures == 0) std::puts("guest_call: all tests passed");
    return failures == 0 ? 0 : 1;
}
