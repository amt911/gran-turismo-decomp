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
// The callee must not switch PSP threads (block in the kernel): a native frame can't be suspended,
// so a switch stops the runtime with an explanation instead of corrupting the scheduler.
std::uint32_t call_guest(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &caller, std::uint32_t address,
                         std::initializer_list<std::uint32_t> arguments);

} // namespace gt
