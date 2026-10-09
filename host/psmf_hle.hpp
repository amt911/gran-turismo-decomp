// scePsmf HLE (host/psmf_hle.cpp).
#pragma once

#include "psprecomp/runtime.hpp"

namespace gt::psmf {

// Registers GT's scePsmf imports. Call from the GT kernel additions.
void install_hle(psprecomp::Runtime &runtime);

} // namespace gt::psmf
