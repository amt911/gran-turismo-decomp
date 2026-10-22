#include "display_phase.hpp"

#include <cmath>

namespace gt {

namespace {
constexpr unsigned kSeedSamples = 16u;      // gaps looked at before the period is seeded from the shortest
constexpr unsigned kLockSamples = 60u;      // tracked completions after the seed before the lock is trusted
constexpr unsigned kMaxOutliers = 20u;      // consecutive off-grid completions before re-seeding (the grid moved)
constexpr double kMinGapNs = 2'000'000.0;   // shorter gaps are two completions of one refresh
constexpr double kPhaseGain = 0.1;
constexpr double kPeriodGain = 0.002;
constexpr double kLockedErrorNs = 1'000'000.0;
} // namespace

void DisplayPhase::on_shown(Clock::time_point when) {
    std::lock_guard lock(mutex_);
    if (samples_ < kSeedSamples) {
        if (samples_ > 0u) {
            const std::int64_t gap = (when - origin_).count();
            if (gap > kMinGapNs && (min_gap_ns_ == 0 || gap < min_gap_ns_)) min_gap_ns_ = gap;
        }
        origin_ = when;
        if (++samples_ == kSeedSamples) {
            if (min_gap_ns_ == 0) {  // every completion on one refresh: nothing to seed from yet
                samples_ = 1u;
                return;
            }
            period_ns_ = static_cast<double>(min_gap_ns_);
            error_ns_ = period_ns_ / 8.0;
        }
        return;
    }
    // The shortest gap overshoots the period by up to the waiter's wakeup jitter; the loop pulls it in.
    const double since = static_cast<double>((when - origin_).count());
    const double refreshes = std::round(since / period_ns_);
    const double error = since - refreshes * period_ns_;
    if (std::abs(error) > period_ns_ / 4.0) {
        if (++outliers_ > kMaxOutliers) {  // the grid moved: learn it again
            samples_ = 1u;
            outliers_ = 0u;
            min_gap_ns_ = 0;
            origin_ = when;
        }
        return;
    }
    outliers_ = 0u;
    origin_ += std::chrono::nanoseconds(std::llround(refreshes * period_ns_ + kPhaseGain * error));
    if (refreshes >= 1.0) period_ns_ += kPeriodGain * error / refreshes;
    error_ns_ += 0.05 * (std::abs(error) - error_ns_);
    ++samples_;
}

bool DisplayPhase::locked() const {
    std::lock_guard lock(mutex_);
    return samples_ >= kSeedSamples + kLockSamples && error_ns_ < kLockedErrorNs;
}

std::chrono::nanoseconds DisplayPhase::period() const {
    std::lock_guard lock(mutex_);
    return std::chrono::nanoseconds(std::llround(period_ns_));
}

std::optional<DisplayPhase::Clock::time_point> DisplayPhase::nearest_refresh(Clock::time_point when) const {
    if (!locked()) return std::nullopt;
    std::lock_guard lock(mutex_);
    const double refreshes = std::round(static_cast<double>((when - origin_).count()) / period_ns_);
    return origin_ + std::chrono::nanoseconds(std::llround(refreshes * period_ns_));
}

DisplayPhase &display_phase() {
    static DisplayPhase phase;
    return phase;
}

} // namespace gt
