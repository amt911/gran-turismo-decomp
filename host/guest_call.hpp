// Calling recompiled (or decompiled) guest functions from decompiled code (host/guest_call.cpp).
#pragma once

#include "psprecomp/runtime.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt {

// Runs the guest function at `address` as a nested call and returns its v0. Arguments go in
// a0-a3, t0-t3 (EABI); the callee's stack frame goes below the caller's sp. Everything else starts
// from `caller` and is discarded afterwards: the decompiled caller keeps its own state in C++.
//
// When the callee blocks (switches PSP threads), the caller's fiber is suspended and resumed once
// its thread returns (host/fiber.hpp). Outside a fiber — the harness's isolated diff runs, tests —
// a switch stops the runtime with an explanation instead.
std::uint32_t call_guest(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &caller, std::uint32_t address,
                         std::initializer_list<std::uint32_t> arguments);

// Room on the guest stack for locals a guest callee needs the address of (a smart pointer it fills,
// an output struct). Lowers ctx's sp by `bytes` (16-aligned) for as long as it lives, like the
// original's frame; the harness ignores that dead stack when comparing.
class GuestFrame {
public:
    GuestFrame(psprecomp::AllegrexContext &ctx, std::uint32_t bytes)
        : ctx_(ctx), saved_sp_(ctx.gpr[29]) {
        ctx_.gpr[29] = (saved_sp_ - bytes) & ~15u;
    }
    ~GuestFrame() { ctx_.gpr[29] = saved_sp_; }
    GuestFrame(const GuestFrame &) = delete;
    GuestFrame &operator=(const GuestFrame &) = delete;
    std::uint32_t at(std::uint32_t offset) const { return ctx_.gpr[29] + offset; }

private:
    psprecomp::AllegrexContext &ctx_;
    std::uint32_t saved_sp_;
};

} // namespace gt
