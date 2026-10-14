// Unit tests for the uniform internal-resolution scale (lcs_render_config.hpp, GT patch 0021). Plain
// executable, run by CTest.
#include "lcs_render_config.hpp"

#include <cstdio>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

bool is(lcs::InternalResolutionDimensions d, std::uint32_t w, std::uint32_t h) {
    return d.width == w && d.height == h;
}

} // namespace

int main() {
    using lcs::uniform_internal_resolution;
    check(is(uniform_internal_resolution(480u, 272u, 8u), 480u, 272u), "a PSP-sized window renders at x1");
    check(is(uniform_internal_resolution(960u, 544u, 8u), 960u, 544u), "an exact x2 window renders at x2");
    check(is(uniform_internal_resolution(1920u, 1080u, 8u), 1920u, 1088u),
          "1080p needs x4: the smallest uniform scale that covers both sides");
    check(is(uniform_internal_resolution(2560u, 1440u, 8u), 2880u, 1632u), "1440p needs x6");
    check(is(uniform_internal_resolution(1920u, 1080u, 2u), 960u, 544u), "the cap limits the scale (weak GPUs)");
    check(is(uniform_internal_resolution(8000u, 8000u, 8u), 3840u, 2176u), "never above x8");
    check(is(uniform_internal_resolution(100u, 50u, 8u), 480u, 272u), "never below x1");
    check(is(uniform_internal_resolution(1920u, 1080u, 0u), 480u, 272u), "a zero cap means x1");
    // GT patch 0034: the GPU-budget scale cap.
    using lcs::budget_scale_cap;
    check(budget_scale_cap(4u, 4u, 8u, 14.0, 10u, 30.0) == 3u, "over budget: one whole scale down");
    check(budget_scale_cap(1u, 1u, 8u, 40.0, 10u, 30.0) == 1u, "never below x1");
    check(budget_scale_cap(3u, 3u, 8u, 6.0, 10u, 30.0) == 3u,
          "x4 would cost 6 * (4/3)^2 = 10.7 ms, over 70 % of 10: stay");
    check(budget_scale_cap(2u, 2u, 8u, 2.0, 10u, 30.0) == 3u, "x3 would cost 4.5 ms: one up");
    check(budget_scale_cap(2u, 2u, 8u, 2.0, 10u, 3.0) == 2u, "a change 3 s ago: not up yet");
    check(budget_scale_cap(8u, 8u, 8u, 1.0, 10u, 30.0) == 8u, "never above the configured maximum");
    check(budget_scale_cap(4u, 4u, 8u, 9.9, 10u, 30.0) == 4u, "just inside the budget: stay");
    check(budget_scale_cap(4u, 4u, 8u, 50.0, 0u, 30.0) == 8u, "budget 0: off, the configured maximum");
    if (failures == 0) std::puts("internal_resolution: all tests passed");
    return failures == 0 ? 0 : 1;
}
