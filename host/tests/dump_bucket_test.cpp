// Unit tests for host/dump_bucket.hpp: the bucket rule of GT_FRAME_DUMP. Plain executable, run by CTest.
#include "dump_bucket.hpp"

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
std::vector<std::uint64_t> dumped(const std::vector<std::uint64_t> &presents, std::uint64_t every) {
    gt::DumpBucket bucket;
    std::vector<std::uint64_t> out;
    for (const std::uint64_t v : presents)
        if (bucket.due(v, every)) out.push_back(v);
    return out;
}
} // namespace

int main() {
    // 60 fps: every vblank presents; exact multiples are dumped.
    std::vector<std::uint64_t> all;
    for (std::uint64_t v = 1; v <= 90; ++v) all.push_back(v);
    check((dumped(all, 30) == std::vector<std::uint64_t>{1, 30, 60, 90}), "60 fps: first present of each bucket (1, 30, 60, 90)");
    // 30 fps on odd vblanks only: an exact modulo would never fire, the bucket rule still dumps once per 30 vblanks.
    std::vector<std::uint64_t> odd;
    for (std::uint64_t v = 1; v <= 119; v += 2) odd.push_back(v);
    check((dumped(odd, 30) == std::vector<std::uint64_t>{1, 31, 61, 91}), "odd-only presents: 1, 31, 61, 91 (a modulo would dump nothing)");
    // Two runs whose presents are the same game frames dump the same vblanks; a dropped present loses only that vblank in one run.
    std::vector<std::uint64_t> a, b;
    for (std::uint64_t v = 2; v <= 120; v += 2) a.push_back(v);
    b = a;
    b.erase(b.begin() + 14);  // the present at vblank 30 is dropped in run b
    check((dumped(a, 30) == std::vector<std::uint64_t>{2, 30, 60, 90, 120}), "run a dumps 2, 30, 60, 90, 120");
    check((dumped(b, 30) == std::vector<std::uint64_t>{2, 32, 60, 90, 120}), "run b (present 30 dropped) dumps 32 in that bucket, the rest the same");
    // A gap (loading screen: no presents for several buckets) dumps once afterwards, not once per skipped bucket.
    check((dumped({10, 11, 500, 501}, 30) == std::vector<std::uint64_t>{10, 500}), "a long gap yields one dump");
    // every == 0 is treated as 1 (every present).
    check((dumped({5, 6, 7}, 0) == std::vector<std::uint64_t>{5, 6, 7}), "every 0 -> every present");
    return failures == 0 ? 0 : 1;
}
