// Unit tests for gt::DisplayPhase (host/display_phase.cpp) and the display lock of gt::VblankPacer. Plain executable,
// run by CTest.
#include "display_phase.hpp"
#include "vblank_pacer.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
using Clock = gt::DisplayPhase::Clock;
using us = std::chrono::microseconds;
using ns = std::chrono::nanoseconds;
constexpr std::int64_t kRefreshNs = 16'653'000;  // the laptop's panel: 60.05 Hz
constexpr std::uint64_t kGuestPeriodUs = 16683u;  // the PSP: 59.94 Hz
const Clock::time_point t0{std::chrono::seconds(100)};

// A screen-side completion: refresh `n` of a grid starting at t0 + phase, seen by the waiter `late` after it.
Clock::time_point refresh(std::int64_t n, ns phase = ns(0), std::int64_t period_ns = kRefreshNs) {
    return t0 + phase + ns(n * period_ns);
}

ns abs_ns(ns d) { return d < ns(0) ? -d : d; }

// Feeds a mailbox-like pattern: mostly every refresh, some refreshes skipped, some ids completing twice in one refresh,
// each seen 0-400 us late (the waiter thread's wakeup).
void feed(gt::DisplayPhase &phase, std::int64_t from, std::int64_t to, ns grid_phase = ns(0),
          std::int64_t period_ns = kRefreshNs) {
    unsigned seed = 7u;
    for (std::int64_t n = from; n < to; ++n) {
        seed = seed * 1103515245u + 12345u;
        const unsigned roll = (seed >> 16) % 8u;
        if (roll == 0u) continue;  // a refresh that showed nothing new
        const ns late = ns(static_cast<std::int64_t>((seed >> 8) % 400u) * 1000);
        phase.on_shown(refresh(n, grid_phase, period_ns) + late);
        if (roll == 1u) phase.on_shown(refresh(n, grid_phase, period_ns) + late + us(30));  // two ids, one refresh
    }
}
} // namespace

int main() {
    {  // nothing is known before the screen has reported anything
        gt::DisplayPhase phase;
        check(!phase.locked() && !phase.nearest_refresh(t0), "no lock without samples");
        phase.on_shown(refresh(0));
        phase.on_shown(refresh(1));
        check(!phase.locked(), "two samples are not a lock");
    }
    {  // a 60.05 Hz panel seen through mailbox completions: locks onto its period and phase
        gt::DisplayPhase phase;
        feed(phase, 0, 600);
        check(phase.locked(), "locks after 10 s of completions");
        check(abs_ns(phase.period() - ns(kRefreshNs)) < ns(3000), "period within 3 us of the panel's");
        const auto r = phase.nearest_refresh(refresh(650) + us(3000));
        check(r && abs_ns(*r - refresh(650)) < ns(600'000), "nearest refresh within the waiter's wakeup of the true one");
        const auto far = phase.nearest_refresh(refresh(2000) - us(4000));  // 23 s later: no drift in the estimate
        check(far && abs_ns(*far - refresh(2000)) < ns(800'000), "the estimate does not drift");
    }
    {  // a slow compositor that shows most images every other refresh still has the panel's grid underneath
        gt::DisplayPhase phase;
        for (std::int64_t n = 0; n < 1200; n += (n % 8 == 0) ? 1 : 2) phase.on_shown(refresh(n) + us(200));
        check(phase.locked() && abs_ns(phase.period() - ns(kRefreshNs)) < ns(3000), "every other refresh: panel period");
    }
    {  // a 120 Hz panel
        gt::DisplayPhase phase;
        feed(phase, 0, 1200, ns(0), 8'333'000);
        check(phase.locked() && abs_ns(phase.period() - ns(8'333'000)) < ns(3000), "120 Hz panel");
    }
    {  // the grid jumps (a mode change, a window moved to another screen): the lock follows it
        gt::DisplayPhase phase;
        feed(phase, 0, 600);
        feed(phase, 700, 1400, us(7000));
        const auto r = phase.nearest_refresh(refresh(1450, us(7000)));
        check(phase.locked() && r && abs_ns(*r - refresh(1450, us(7000))) < ns(600'000), "relocks after a jump");
    }

    // The pacer, locked: each vblank's real time is pulled to the panel's refresh + the offset, 200 us at most per vblank,
    // so the game runs at the panel's rate (60.05 instead of 59.94 vblanks/s) with every present at the same phase.
    {
        gt::DisplayPhase phase;
        feed(phase, -600, 0);
        gt::VblankPacer pacer(kGuestPeriodUs, 1000);
        (void)pacer.on_wait(t0 + us(5000), kGuestPeriodUs, &phase);  // anchors 5 ms after a refresh
        bool capped = true, converged = true;
        Clock::time_point previous = t0 + us(5000);
        for (std::uint64_t v = 2u; v < 3000u; ++v) {
            const auto wait = pacer.on_wait(previous + us(2000), v * kGuestPeriodUs, &phase);
            if (!wait.sleep_until) { converged = false; break; }
            const auto step = *wait.sleep_until - previous;
            capped = capped && abs_ns(step - us(kGuestPeriodUs)) <= ns(200'000);
            if (v > 200u) {  // after the pull-in: refresh + 1 ms, within the phase estimate's error
                const auto r = phase.nearest_refresh(*wait.sleep_until - us(1000));
                converged = converged && r && abs_ns(*wait.sleep_until - (*r + us(1000))) < ns(600'000);
            }
            previous = *wait.sleep_until;
        }
        check(capped, "a locked pacer moves each vblank by 200 us at most");
        check(converged, "a locked pacer keeps every vblank at refresh + offset (no beat)");
    }
    {  // a phase that is not locked leaves the pacer on the wall clock
        gt::DisplayPhase phase;
        gt::VblankPacer pacer(kGuestPeriodUs, 1000);
        (void)pacer.on_wait(t0, kGuestPeriodUs, &phase);
        const auto wait = pacer.on_wait(t0 + us(5000), 2u * kGuestPeriodUs, &phase);
        check(wait.sleep_until == t0 + us(kGuestPeriodUs), "unlocked: plain wall-clock pacing");
    }
    {  // a 144 Hz panel has no whole number of refreshes per vblank: no lock (the pacer would jump between refreshes)
        gt::DisplayPhase phase;
        feed(phase, 0, 1400, ns(0), 6'944'000);
        gt::VblankPacer pacer(kGuestPeriodUs, 1000);
        (void)pacer.on_wait(t0 + us(1'000'000), kGuestPeriodUs, &phase);
        const auto wait = pacer.on_wait(t0 + us(1'005'000), 2u * kGuestPeriodUs, &phase);
        check(phase.locked() && wait.sleep_until == t0 + us(1'000'000 + kGuestPeriodUs), "144 Hz: wall-clock pacing");
    }
    {  // 120 Hz: two refreshes per vblank, locked
        gt::DisplayPhase phase;
        feed(phase, -1200, 0, ns(0), 8'333'000);
        gt::VblankPacer pacer(kGuestPeriodUs, 1000);
        (void)pacer.on_wait(t0 + us(5000), kGuestPeriodUs, &phase);
        const auto wait = pacer.on_wait(t0 + us(7000), 2u * kGuestPeriodUs, &phase);
        check(wait.sleep_until && *wait.sleep_until != t0 + us(5000 + kGuestPeriodUs), "120 Hz: the lock pulls");
    }
    if (failures == 0) std::puts("display_phase: all tests passed");
    return failures == 0 ? 0 : 1;
}
