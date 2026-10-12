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
    if (failures == 0) std::puts("internal_resolution: all tests passed");
    return failures == 0 ? 0 : 1;
}
