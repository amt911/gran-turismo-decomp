// Scripted pad presses (GT_PAD): host/pad_script.cpp.
#pragma once

#include <cstdint>

namespace gt {

void pad_script_tick();            // one vblank wait passed
std::uint32_t scripted_buttons();  // PSP button bits pressed by the script now
std::uint64_t pad_script_vblanks();  // vblank waits so far: the clock GT_PAD times are written in

} // namespace gt
