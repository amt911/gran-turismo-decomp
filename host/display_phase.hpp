// The screen's refresh grid, learned from the times images reach it (host/display_phase.cpp). Fed by the Vulkan
// presenter's VK_KHR_present_wait completions; read by gt::VblankPacer to lock guest vblanks to the screen.
#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>

namespace gt {

// A phase-locked loop on screen-side completion times. Each completion lands on a refresh, seen a little late (the
// waiter thread's wakeup); several can land on one refresh (mailbox: replaced images complete with the one shown) and
// refreshes can show nothing new. The period is seeded from the shortest gap between completions, then the loop tracks
// both period and phase. Thread-safe: completions arrive on the presenter's waiter thread, reads on the guest's.
class DisplayPhase {
public:
    using Clock = std::chrono::steady_clock;

    void on_shown(Clock::time_point when);
    bool locked() const;
    std::chrono::nanoseconds period() const;
    // The refresh nearest `when`, once locked.
    std::optional<Clock::time_point> nearest_refresh(Clock::time_point when) const;

private:
    mutable std::mutex mutex_;
    Clock::time_point origin_{};  // a refresh (the last one a completion landed on)
    double period_ns_{};
    double error_ns_{};           // smoothed |phase error| of the accepted completions
    std::int64_t min_gap_ns_{};   // seeding: the shortest gap seen so far
    unsigned samples_{};
    unsigned outliers_{};         // consecutive completions off the grid
};

// The one screen gt_play presents to (fed by host/play_backends.cpp, read by the vblank pacer in host/gt_kernel.cpp).
DisplayPhase &display_phase();

} // namespace gt
