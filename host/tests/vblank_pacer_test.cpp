// Unit tests for gt::VblankPacer (host/vblank_pacer.cpp). Plain executable, run by CTest.
#include "vblank_pacer.hpp"

#include <cstdio>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
using Clock = gt::VblankPacer::Clock;
using us = std::chrono::microseconds;
constexpr std::uint64_t kPeriod = 16683u;
} // namespace

int main() {
    const Clock::time_point t0{std::chrono::seconds(100)};

    {  // the first wait anchors guest time to the wall clock
        gt::VblankPacer pacer(kPeriod);
        const auto wait = pacer.on_wait(t0, kPeriod);
        check(!wait.sleep_until && wait.skipped_vblanks == 0u && !pacer.host_late(), "first wait anchors");
    }
    {  // an early wait sleeps until its vblank's real time
        gt::VblankPacer pacer(kPeriod);
        (void)pacer.on_wait(t0, kPeriod);
        const auto wait = pacer.on_wait(t0 + us(5000), 2u * kPeriod);
        check(wait.sleep_until == t0 + us(kPeriod) && wait.skipped_vblanks == 0u && !pacer.host_late(),
              "early wait sleeps until the vblank");
        check(wait.lag == us(5000 - static_cast<long>(kPeriod)), "an early wait's lag is negative");
    }
    {  // GT's second wait of the same vblank (same guest instant) is not lag: the first one already slept until it
        gt::VblankPacer pacer(kPeriod);
        (void)pacer.on_wait(t0, kPeriod);
        (void)pacer.on_wait(t0 + us(5000), 2u * kPeriod);  // sleeps until t0 + period
        const auto second = pacer.on_wait(t0 + us(kPeriod + 3000), 2u * kPeriod);
        check(!second.sleep_until && second.skipped_vblanks == 0u, "second wait of a vblank does not sleep or skip");
        check(!second.lag, "the second wait of a vblank reports no lag");
        check(!pacer.host_late(), "second wait of a vblank 3 ms after it is not 'host late'");
    }
    {  // a first wait 2 ms after its vblank: caught up by not sleeping, not worth a skipped frame
        gt::VblankPacer pacer(kPeriod);
        (void)pacer.on_wait(t0, kPeriod);
        const auto wait = pacer.on_wait(t0 + us(kPeriod + 2000), 2u * kPeriod);
        check(!wait.sleep_until && wait.skipped_vblanks == 0u && !pacer.host_late(), "2 ms late is not host late");
        check(wait.lag == us(2000), "a measuring wait reports its lag (wall time past its vblank)");
    }
    {  // a first wait that arrives more than 4 ms (a quarter vblank) after its vblank is late
        gt::VblankPacer pacer(kPeriod);
        (void)pacer.on_wait(t0, kPeriod);
        const auto wait = pacer.on_wait(t0 + us(kPeriod + 5000), 2u * kPeriod);
        check(!wait.sleep_until && wait.skipped_vblanks == 0u && pacer.host_late(), "5 ms late sets host late");
        (void)pacer.on_wait(t0 + us(kPeriod + 5500), 2u * kPeriod);
        check(pacer.host_late(), "the second wait leaves the first wait's verdict");
        const auto next = pacer.on_wait(t0 + us(kPeriod + 9000), 3u * kPeriod);
        check(next.sleep_until && !pacer.host_late(), "an on-time first wait clears it");
    }
    {  // 4 to 6 vblanks behind: skip them; more: skip 6 and re-anchor
        gt::VblankPacer pacer(kPeriod);
        (void)pacer.on_wait(t0, kPeriod);
        const auto five = pacer.on_wait(t0 + us(6u * kPeriod), 2u * kPeriod);
        check(five.skipped_vblanks == 5u, "five vblanks behind skips five");
        gt::VblankPacer stalled(kPeriod);
        (void)stalled.on_wait(t0, kPeriod);
        const auto stall = stalled.on_wait(t0 + us(20u * kPeriod), 2u * kPeriod);
        check(stall.skipped_vblanks == 6u, "a long stall skips six");
        const auto after = stalled.on_wait(t0 + us(20u * kPeriod + 1000), 9u * kPeriod);
        check(after.sleep_until == t0 + us(20u * kPeriod + kPeriod), "re-anchored after the stall");
    }
    if (failures == 0) std::puts("vblank_pacer: all tests passed");
    return failures == 0 ? 0 : 1;
}
