// Unit tests for gt::DirtyPages (host/dirty_pages.cpp): write tracking by page protection. Plain executable, run
// by CTest.
#include "dirty_pages.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

bool has(const std::vector<std::size_t> &pages, std::size_t page) {
    return std::find(pages.begin(), pages.end(), page) != pages.end();
}

} // namespace

int main() {
    // Like the guest RAM: a vector, so its data needn't start on a page boundary.
    std::vector<std::uint8_t> ram(64u * 4096u + 100u);
    for (std::size_t i = 0; i < ram.size(); ++i) ram[i] = static_cast<std::uint8_t>(i * 7u);
    const std::vector<std::uint8_t> entry = ram;
    gt::DirtyPages tracker;
    const std::size_t page = gt::DirtyPages::kPageSize;

    tracker.begin(ram.data(), ram.size());
    check(tracker.written().empty(), "nothing written yet");
    std::uint8_t sum = 0;
    for (std::size_t i = 0; i < ram.size(); i += 512u) sum = static_cast<std::uint8_t>(sum + ram[i]);  // reads don't count
    check(tracker.written().empty(), "reads are not writes");
    (void)sum;
    ram[5u * page + 3u] = 0xEEu;
    ram[5u * page + 9u] = 0xEFu;
    ram[40u * page] = 0x11u;
    ram[ram.size() - 1u] = 0x22u;  // the last, partial page
    ram[0] = 0x33u;                // the first, partial page
    {
        const auto w = tracker.written();
        check(w.size() == 4u, "each written page once");
        const std::size_t first_page = tracker.page_of(ram.data());
        check(has(w, first_page) && has(w, tracker.page_of(ram.data() + 5u * page + 3u)) &&
                  has(w, tracker.page_of(ram.data() + 40u * page)) && has(w, tracker.page_of(ram.data() + ram.size() - 1u)),
              "the written pages are the ones listed");
    }
    // The pristine copy holds the bytes from before the first write.
    std::vector<std::uint8_t> before(ram.size());
    tracker.copy_pristine(before.data());
    check(std::memcmp(before.data(), entry.data(), ram.size()) == 0, "the pristine image is the entry state");

    // Restore, re-arm, and a second run's writes are listed apart from the first's.
    tracker.restore_pristine();
    check(std::memcmp(ram.data(), entry.data(), ram.size()) == 0, "restored to the entry state");
    tracker.rearm();
    check(tracker.written().empty(), "re-armed: nothing written in this run yet");
    ram[7u * page] = 0x44u;
    check(tracker.written().size() == 1u && has(tracker.written(), tracker.page_of(ram.data() + 7u * page)),
          "the second run's page");
    check(tracker.ever_written().size() == 5u, "both runs' pages together");

    // A write from another thread is caught too (a host thread writing guest RAM during a case).
    std::thread([&] { ram[20u * page + 1u] = 0x55u; }).join();
    check(has(tracker.written(), tracker.page_of(ram.data() + 20u * page)), "a write from another thread");

    // Everything at once, for code that writes through system calls (they fail instead of faulting).
    tracker.materialize();
    ram[30u * page] = 0x66u;  // no fault: writable
    check(tracker.ever_written().size() == tracker.page_count(), "materialized: every page listed");
    tracker.copy_pristine(before.data());
    check(std::memcmp(before.data() + 30u * page, entry.data() + 30u * page, page) == 0,
          "materialized pages keep their entry bytes");

    tracker.end();
    ram[50u * page] = 0x77u;  // writable again, nothing tracked
    check(tracker.ever_written().empty(), "ended: nothing listed");

    // A second case starts clean.
    tracker.begin(ram.data(), ram.size());
    ram[2u * page] = 0x88u;
    check(tracker.written().size() == 1u, "a new case lists only its own writes");
    tracker.end();

    if (failures == 0) std::puts("dirty_pages_test: all passed");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
