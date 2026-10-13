// Isolation of harness cases from kernel state (host/gt_kernel.cpp, host/diff_harness.cpp).
//
// The diff harness runs each sampled call twice (original, then decompiled) from the same RAM
// snapshot. Imports it lets run inside a case must keep all their state in guest RAM, which it
// restores. An LwMutex is like that — until a thread waits on it: then locking queues the caller and
// unlocking wakes a waiter, which is scheduler state on the host. While a case is isolated the kernel
// refuses such operations (doing nothing) and marks the case broken; the harness drops it and lets
// the call run for real. The runtime's starvation tick (virtual time, wake-ups, timer preemption)
// is suspended for the same reason.
#pragma once

namespace gt {

void set_kernel_isolated(bool isolated) noexcept;
bool kernel_isolated() noexcept;

// Called by the kernel instead of an operation that would touch host-side scheduler state; `reason` (a
// string literal) is what the harness reports.
void break_kernel_isolation(const char *reason = "LwMutex contention") noexcept;
const char *kernel_isolation_reason() noexcept;
// Whether that happened since set_kernel_isolated(true).
bool kernel_isolation_broken() noexcept;

// Waits until the GE worker thread has executed every queued display list. GE lists write guest
// RAM (block transfers, render targets in RAM), so a case must not start while one is running; none
// can start during a case (sceGeListEnQueue isn't replay-safe). Implemented in host/gt_kernel.cpp;
// tests that don't link the kernel provide their own.
void wait_for_ge_idle();

} // namespace gt
