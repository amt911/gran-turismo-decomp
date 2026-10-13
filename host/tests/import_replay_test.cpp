// Unit tests for the diff harness's import record & replay log (host/import_replay.cpp). Synthetic data
// only. Plain executable, run by CTest.
#include "import_replay.hpp"

#include <array>
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

std::array<std::uint32_t, 8> args(std::uint32_t a0, std::uint32_t a1 = 0u, std::uint32_t a2 = 0u) {
    return {a0, a1, a2, 0u, 0u, 0u, 0u, 0u};
}

gt::ImportRecord record(std::uint32_t stub, unsigned arity, std::array<std::uint32_t, 8> a) {
    gt::ImportRecord r;
    r.stub = stub;
    r.arity = arity;
    r.args = a;
    return r;
}

} // namespace

int main() {
    using gt::ImportLog;
    {
        std::vector<std::uint8_t> before(3u * 4096u, 0u), after = before;
        after[10] = 1u;
        after[11] = 2u;
        after[4096u * 2u + 5u] = 9u;
        const auto runs = gt::ram_changes(before, after, 0x08000000u);
        check(runs.size() == 2u, "two separate changed runs");
        check(runs.size() == 2u && runs[0].address == 0x0800000Au && runs[0].bytes == std::vector<std::uint8_t>({1, 2}),
              "first run: address and the changed bytes only");
        check(runs.size() == 2u && runs[1].address == 0x08002005u && runs[1].bytes.size() == 1u,
              "second run, in a later page");
        std::vector<std::uint8_t> target = before;
        gt::apply_ram(target, 0x08000000u, runs);
        check(target == after, "applying the runs reproduces the after image");
        check(gt::ram_changes(before, before, 0x08000000u).empty(), "identical images: no runs");
    }
    {
        std::array<std::uint32_t, 32> before{}, after{};
        before[2] = 5u;
        after = before;
        after[2] = 7u;
        after[3] = 1u;
        const auto changed = gt::changed_registers(before, after);
        check(changed.size() == 2u && changed[0].first == 2u && changed[0].second == 7u && changed[1].first == 3u,
              "changed registers: index and new value, in order");
        std::array<std::uint32_t, 32> target{};
        target[4] = 42u;
        gt::apply_registers(target, changed);
        check(target[2] == 7u && target[3] == 1u && target[4] == 42u, "apply sets only the changed registers");
    }
    {
        ImportLog log;
        log.append(record(0x08C7AA18u, 2u, args(0x1000u, 0x20u, 0xDEADu)));
        log.append(record(0x08C7A9E8u, 0u, args(0u)));
        auto first = log.next(0x08C7AA18u, args(0x1000u, 0x20u, 0x1234u));
        check(first.record != nullptr && first.error.empty(), "same stub and same arguments up to the arity match");
        auto second = log.next(0x08C7A9E8u, args(0x77u));
        check(second.record != nullptr, "a 0-arity import ignores the argument registers");
        check(!log.leftover(), "all records consumed: no leftover");
        auto extra = log.next(0x08C7A9E8u, args(0u));
        check(extra.record == nullptr && !extra.error.empty(), "a call past the end is an error");
    }
    {
        ImportLog log;
        log.append(record(0x08C7AA18u, 1u, args(0x1000u)));
        auto wrong_stub = log.next(0x08C7A9E8u, args(0x1000u));
        check(wrong_stub.record == nullptr && !wrong_stub.error.empty(), "another import is an error");
        ImportLog log2;
        log2.append(record(0x08C7AA18u, 1u, args(0x1000u)));
        auto wrong_arg = log2.next(0x08C7AA18u, args(0x1004u));
        check(wrong_arg.record == nullptr && !wrong_arg.error.empty(), "a different argument is an error");
        ImportLog log3;
        log3.append(record(0x08C7AA18u, 1u, args(0x1000u)));
        check(log3.leftover().has_value(), "an unconsumed record is reported as leftover");
        log3.clear();
        check(!log3.leftover() && log3.size() == 0u, "clear empties the log");
    }
    if (failures == 0) std::puts("import_replay: all tests passed");
    return failures == 0 ? 0 : 1;
}
