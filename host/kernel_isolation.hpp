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

// Called by the kernel instead of an operation that would touch host-side scheduler state.
void break_kernel_isolation() noexcept;
// Whether that happened since set_kernel_isolated(true).
bool kernel_isolation_broken() noexcept;

} // namespace gt
