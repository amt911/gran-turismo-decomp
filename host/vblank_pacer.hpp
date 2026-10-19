// Pacing guest vblanks to the wall clock (host/vblank_pacer.cpp), used by the vblank waits in host/gt_kernel.cpp.
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

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

    explicit VblankPacer(std::uint64_t period_us) : period_us_(period_us) {}

    // A wait for the vblank at guest time `next_vblank_us`, made at wall time `now`.
    Wait on_wait(Clock::time_point now, std::uint64_t next_vblank_us);
    // Whether the last wait that measured the lag (the first wait of its vblank) found the host more than 4 ms late.
    bool host_late() const { return host_late_; }

private:
    std::uint64_t period_us_;
    Clock::time_point anchor_wall_{};
    std::uint64_t anchor_guest_us_{};
    std::uint64_t last_target_us_{~std::uint64_t{0}};  // the vblank the last measuring wait was for
    bool anchored_{};
    bool host_late_{};
};

} // namespace gt
