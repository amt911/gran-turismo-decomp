// Native fibers for decompiled code (host/fiber.cpp).
//
// Recompiled code survives a PSP thread switch because all its state is in guest registers and
// memory: it simply returns to the dispatcher. Decompiled C++ keeps state in native locals, so when a
// guest function it calls blocks in the kernel, its native frame must be kept alive until that PSP
// thread runs again. Each decompiled call from the dispatcher therefore runs on its own fiber (a
// native stack); gt::call_guest suspends the fiber on a thread switch and the fiber resumes when the
// blocked thread returns to the fiber's private return address.
#pragma once

#include "decomp_registry.hpp"
#include "psprecomp/runtime.hpp"

#include <cstdint>

namespace gt::fiber {

// Runs a decompiled function the way the dispatcher expects (return to ra, or continue at a
// gt::tail_call target), on a fiber when called from the dispatcher; inline when already on one.
// `ctx` must be the runtime's live context. Returns with ctx holding whatever thread runs next.
void run(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, DecompiledFn fn, std::uint32_t entry);

// For gt::call_guest: whether the caller runs on a fiber, the return address guest code must use to
// get back to it, and the suspension point. suspend() is called with `local` holding the context of
// the thread that runs next; it returns once the fiber's thread came back to the return address,
// with `local` holding that thread's context.
bool on_fiber() noexcept;
std::uint32_t return_address() noexcept;
void suspend(psprecomp::AllegrexContext &local);

// Fibers created and suspensions so far (for the --decomp report).
std::size_t created() noexcept;
std::size_t max_stack_used() noexcept;  // bytes, the deepest any fiber went (approximate, 64-byte steps)
std::uint64_t suspensions() noexcept;

} // namespace gt::fiber
