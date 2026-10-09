// sceLibFont HLE (host/font_hle.cpp).
#pragma once

#include "psprecomp/runtime.hpp"

namespace gt::font {

// Registers GT's sceLibFont imports. Call from the GT kernel additions.
void install_hle(psprecomp::Runtime &runtime);

} // namespace gt::font
