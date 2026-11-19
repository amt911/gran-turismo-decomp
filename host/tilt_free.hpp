// Mod API v7 tilt_free: the named form of the rollover mod's flag trick (docs/FACTS.md "Rollovers: the dormant flag and the flip state machine").
// The game clamps a car's two tilt angles to +-35 degrees unless the word at 0x08CF2F08 (read by 0x0894CCB8) is non-zero. The host hooks that reader for every
// mod that declared `api >= 7` and answers 1 to the readers inside the body step of a car the mod freed: the clamp and the Euler-angle wrap, never the dormant
// flip machine (it sinks the car through the road) nor the contact step (its decompiled form differs with the flag set).
#pragma once

#include "mod_hooks.hpp"

#include <cstdint>
#include <string>

namespace psprecomp {
class Runtime;
}

namespace gt::mods::tilt {

constexpr std::uint32_t kFlagReader = 0x0894CCB8u, kBodyStep = 0x08956DC8u, kContactStep = 0x08945F6Cu;
// `--decomp off`: the reader's `ra` names its caller (call site + 8): the tilt clamp and the Euler-angle wrap. `--decomp on`: `ra` is a small site number and the
// extent of the body-step hook covers the callers instead (FACTS "Hook mechanics measured for the rollover mod").
constexpr std::uint32_t kClampRa = 0x0893E5ACu, kWrapRa = 0x08956BF0u;

// The decision, pure so a test can pin it: does the reader called from `ra` get 1?
bool lift_reader(std::uint32_t ra, bool inside_freed_body_step, bool body_car_freed);

// api tilt_free: frees (on != 0) or restores the clamp for the body `car` (a handle). 0 = ok, 1 = null handle or the mod declared api < 7.
int api_tilt_free(GtMod *self, GtModCtx *ctx, std::uint32_t car, int on);
bool freed(std::uint32_t car);  // any active mod frees this body

// The loader calls this for a mod with api >= 7, before its gt_mod_init: installs the three hooks owned by `mod`.
bool install(psprecomp::Runtime &rt, GtMod *mod, std::string &error);
void forget(const GtMod *mod);  // a mod that is removed
void clear_for_tests();

}  // namespace gt::mods::tilt
