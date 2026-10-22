// Pacing guest vblanks to the wall clock (host/vblank_pacer.cpp), used by the vblank waits in host/gt_kernel.cpp.
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "display_phase.hpp"

namespace gt {

// Keeps guest time in step with the wall clock: the vblank a wait is for is due at
// anchor + (its guest time - anchor's guest time). A short lag is caught up by not sleeping (and by gt_play's
// frameskip, which reads host_late()); 4-6 vblanks behind are skipped; a longer stall skips 6 and re-anchors.
class VblankPacer {
public:
    using Clock = std::chrono::steady_clock;
    struct Wait {
        std::optional<Clock::time_point> sleep_until;  // sleep until then (the vblank's real time)
        std::uint32_t skipped_vblanks{};               // guest vblanks to jump over
        // For a wait that measures the lag (the first of its vblank): wall time past the vblank's due time (negative:
        // early). For the perf log.
        std::optional<std::chrono::microseconds> lag{};
    };

    // `display_offset_us`: with a locked display (on_wait), how long after a screen refresh each vblank is due.
    explicit VblankPacer(std::uint64_t period_us, std::int64_t display_offset_us = 1000)
        : period_us_(period_us), display_offset_us_(display_offset_us) {}

    // A wait for the vblank at guest time `next_vblank_us`, made at wall time `now`. With `display` locked to a screen
    // that shows a whole number of refreshes per vblank, the vblanks follow its refreshes instead of the wall clock.
    Wait on_wait(Clock::time_point now, std::uint64_t next_vblank_us, const DisplayPhase *display = nullptr);
    // Whether the last wait that measured the lag (the first wait of its vblank) found the host more than 4 ms late.
    bool host_late() const { return host_late_; }

private:
    std::uint64_t period_us_;
    std::int64_t display_offset_us_;
    Clock::time_point anchor_wall_{};
    std::uint64_t anchor_guest_us_{};
    std::uint64_t last_target_us_{~std::uint64_t{0}};  // the vblank the last measuring wait was for
    bool anchored_{};
    bool host_late_{};
};

} // namespace gt
