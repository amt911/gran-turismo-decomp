// Unit tests for host/image_ppm.cpp: the box downsample GT_GPU_DUMP uses to bring the GPU backend's
// frame (internal resolution) down to the PSP's 480x272. Synthetic images only. Plain executable,
// run by CTest.
#include "image_ppm.hpp"

#include <cstddef>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

std::vector<std::byte> rgba(std::size_t pixels, std::initializer_list<int> pixel) {
    std::vector<std::byte> out;
    for (std::size_t i = 0; i < pixels; ++i)
        for (int c : pixel) out.push_back(static_cast<std::byte>(c));
    return out;
}

} // namespace

int main() {
    {  // 4x2: left 2x2 black, right 2x2 (200,100,50) -> 2x1
        std::vector<std::byte> image;
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 4; ++x)
                for (int c = 0; c < 4; ++c) {
                    static constexpr int dark[4] = {0, 0, 0, 255}, light[4] = {200, 100, 50, 255};
                    image.push_back(static_cast<std::byte>(x < 2 ? dark[c] : light[c]));
                }
        const std::vector<std::uint8_t> out = gt::downsample_rgba_box(image, 4u, 2u, 2u, 1u);
        check(out == std::vector<std::uint8_t>{0, 0, 0, 200, 100, 50}, "2x2 blocks average to their colour");
    }
    {  // 960x544 constant -> 480x272 constant
        const std::vector<std::uint8_t> out = gt::downsample_rgba_box(rgba(960u * 544u, {9, 99, 199, 255}), 960u, 544u, 480u, 272u);
        bool all = out.size() == 480u * 272u * 3u;
        for (std::size_t i = 0; all && i < out.size(); i += 3u) all = out[i] == 9 && out[i + 1] == 99 && out[i + 2] == 199;
        check(all, "a constant image stays constant");
    }
    {  // 5x3 -> 2x1: column ranges [0,2.5) and [2.5,5) map to pixels {0,1,2} and {2,3,4} (floor/ceil)
        std::vector<std::byte> image;
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 5; ++x)
                for (int c : {x * 10, 0, 0, 255}) image.push_back(static_cast<std::byte>(c));
        const std::vector<std::uint8_t> out = gt::downsample_rgba_box(image, 5u, 3u, 2u, 1u);
        check(out.size() == 6u && out[0] == 10 && out[3] == 30, "non-integer ratios average the covered pixels");
    }
    {  // same size: identity
        const std::vector<std::uint8_t> out = gt::downsample_rgba_box(rgba(4u, {1, 2, 3, 4}), 2u, 2u, 2u, 2u);
        check(out == std::vector<std::uint8_t>{1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3}, "same size keeps the pixels");
    }
    {  // too small an input yields nothing rather than reading out of bounds
        check(gt::downsample_rgba_box(rgba(3u, {0, 0, 0, 0}), 2u, 2u, 1u, 1u).empty(), "short input is rejected");
    }
    if (failures == 0) std::printf("image_ppm: all tests passed\n");
    return failures == 0 ? 0 : 1;
}
