// Pass-through entry probes for guest functions, enabled with GT_PROBE=addr[,addr...] (host/probes.cpp).
#pragma once

#include "psprecomp/runtime.hpp"

namespace gt {

// Wraps every function listed in GT_PROBE; no-op when the variable is unset. Call after the
// generated code and HLE are registered (and before the decomp harness, which may wrap the same).
void install_probes(psprecomp::Runtime &runtime);

// Prints hit counts for every probe (nothing when GT_PROBE is unset).
void probe_report();
void live_scan(psprecomp::Runtime &rt, std::uint64_t vblank);  // SPIKE, GT_LIVE_SCAN

} // namespace gt
