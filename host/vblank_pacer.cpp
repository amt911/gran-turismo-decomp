#include "vblank_pacer.hpp"

#include <algorithm>
#include <cmath>

namespace gt {

VblankPacer::Wait VblankPacer::on_wait(Clock::time_point now, std::uint64_t next_vblank_us, const DisplayPhase *display) {
    constexpr std::uint64_t kMaxSkippedVblanks = 6u;
    constexpr std::uint64_t kTolerableLagVblanks = 4u;
    constexpr std::int64_t kLateThresholdUs = 4000;
    if (!anchored_ || next_vblank_us < anchor_guest_us_) {
        anchored_ = true;
        anchor_wall_ = now;
        anchor_guest_us_ = next_vblank_us;
        last_target_us_ = next_vblank_us;
        return {};
    }
    // GT waits for each vblank from two threads in the same guest instant: the second wait targets the vblank the first
    // one already slept until, so in real time it always arrives "late" by the work between the two waits. On a slow
    // CPU (a laptop in power-saver, 1.9 GHz) that work exceeds 1 ms and every second wait set host_late, skipping frames
    // with the game 80 % idle (docs/FINDINGS.md, 2026-10-19). Only the first wait of a vblank measures the lag.
    if (next_vblank_us == last_target_us_) return {};
    last_target_us_ = next_vblank_us;
    auto due = anchor_wall_ + std::chrono::microseconds(next_vblank_us - anchor_guest_us_);
    // Locked to the screen: the guest's 59.94 Hz against a 60.05 Hz panel slides each present across the compositor's
    // deadline every 9 s, and while it sits there images are shown late or replaced unseen (stutter; FINDINGS
    // 2026-10-22). Pulling each vblank to refresh + offset, 200 us at most per vblank, runs the game at the panel's rate
    // instead, every present at the same point of the refresh.
    if (display != nullptr && display->locked()) {
        const double ratio = static_cast<double>(period_us_) * 1000.0 / static_cast<double>(display->period().count());
        const double whole = std::round(ratio);
        if (whole >= 1.0 && std::abs(ratio - whole) < 0.01 * whole) {
            const auto offset = std::chrono::microseconds(display_offset_us_);
            if (const auto refresh = display->nearest_refresh(due - offset)) {
                constexpr std::chrono::nanoseconds kMaxPull = std::chrono::microseconds(200);
                const auto pull = std::clamp<std::chrono::nanoseconds>(*refresh + offset - due, -kMaxPull, kMaxPull);
                anchor_wall_ += pull;
                due += pull;
            }
        }
    }
    // Late = more than a quarter vblank behind. A lag of 1-2 ms (seen with the game 60 % idle) is caught up on the next
    // frame by not sleeping; skipping a frame for it is a visible stutter that buys nothing (FINDINGS 2026-10-19).
    host_late_ = now - due > std::chrono::microseconds(kLateThresholdUs);
    const auto lag = std::chrono::duration_cast<std::chrono::microseconds>(now - due);
    if (now < due) return {due, 0u, lag};
    const auto behind = static_cast<std::uint64_t>((now - due) / std::chrono::microseconds(period_us_));
    if (behind < kTolerableLagVblanks) return {std::nullopt, 0u, lag};
    if (behind <= kMaxSkippedVblanks) return {std::nullopt, static_cast<std::uint32_t>(behind), lag};
    anchor_wall_ = now;
    anchor_guest_us_ = next_vblank_us + kMaxSkippedVblanks * period_us_;
    return {std::nullopt, static_cast<std::uint32_t>(kMaxSkippedVblanks), lag};
}

} // namespace gt
