// Two optional host callbacks the kernel HLE (host/gt_kernel.cpp) calls, set by gt_play only (gt_native leaves them null):
// the pad filter (what the game reads from sceCtrlReadBufferPositive) and the per-vblank tick (the mod API v2 tick).
#pragma once

#include "psprecomp/allegrex_context.hpp"

#include <cstdint>

namespace psprecomp {
class Runtime;
}

namespace gt {

// Called for every pad read the game makes, with the game's vblank count: may change the buttons and the analog stick the game
// is about to see. A read that does not call it (no filter set) gives the raw host + GT_PAD state unchanged.
using PadFilterFn = void (*)(std::uint64_t vblank, std::uint32_t &buttons, std::uint8_t &analog_x, std::uint8_t &analog_y);
void set_pad_filter(PadFilterFn filter);
PadFilterFn pad_filter();

// Called once per game vblank (the first vblank wait to see a new vblank count), on that wait's guest thread.
using VblankTickFn = void (*)(psprecomp::Runtime &runtime, psprecomp::AllegrexContext &ctx, std::uint64_t vblank);
void set_vblank_tick(VblankTickFn tick);
VblankTickFn vblank_tick();

}  // namespace gt
