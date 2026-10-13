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

gt::ImportRecord record(std::uint32_t stub, unsigned mask, std::array<std::uint32_t, 8> a) {
    gt::ImportRecord r;
    r.stub = stub;
    r.arg_mask = static_cast<std::uint8_t>(mask);
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
        log.append(record(0x08C7AA18u, 0x03u, args(0x1000u, 0x20u, 0xDEADu)));
        log.append(record(0x08C7A9E8u, 0x00u, args(0u)));
        auto first = log.next(0x08C7AA18u, args(0x1000u, 0x20u, 0x1234u));
        check(first.record != nullptr && first.error.empty(), "same stub and same masked arguments match");
        auto second = log.next(0x08C7A9E8u, args(0x77u));
        check(second.record != nullptr, "an import without arguments ignores the argument registers");
        check(!log.leftover(), "all records consumed: no leftover");
        auto extra = log.next(0x08C7A9E8u, args(0u));
        check(extra.record == nullptr && !extra.error.empty(), "a call past the end is an error");
    }
    {
        ImportLog log;
        log.append(record(0x08C7AA18u, 0x01u, args(0x1000u)));
        auto wrong_stub = log.next(0x08C7A9E8u, args(0x1000u));
        check(wrong_stub.record == nullptr && !wrong_stub.error.empty(), "another import is an error");
        ImportLog log2;
        log2.append(record(0x08C7AA18u, 0x01u, args(0x1000u)));
        auto wrong_arg = log2.next(0x08C7AA18u, args(0x1004u));
        check(wrong_arg.record == nullptr && !wrong_arg.error.empty(), "a different argument is an error");
        ImportLog log3;
        log3.append(record(0x08C7AA18u, 0x01u, args(0x1000u)));
        check(log3.leftover().has_value(), "an unconsumed record is reported as leftover");
        log3.clear();
        check(!log3.leftover() && log3.size() == 0u, "clear empties the log");
    }
    {
        ImportLog log;  // sceIoLseek(fd, <padding>, offset lo, offset hi, whence): a1 is not compared
        log.append(record(0x08C7A880u, 0x1Du, {3u, 0xAAAAu, 0x100u, 0u, 0u, 0u, 0u, 0u}));
        check(log.next(0x08C7A880u, {3u, 0xBBBBu, 0x100u, 0u, 0u, 0u, 0u, 0u}).record != nullptr,
              "registers outside the mask are not compared");
        ImportLog log2;
        log2.append(record(0x08C7A880u, 0x1Du, {3u, 0u, 0x100u, 0u, 0u, 0u, 0u, 0u}));
        check(log2.next(0x08C7A880u, {3u, 0u, 0x100u, 0u, 1u, 0u, 0u, 0u}).record == nullptr,
              "a register inside the mask (t0) is compared");
    }
    {
        gt::KernelReplayState before, after, current;
        before.next_stack_top = 0x0A000000u;
        before.next_uid = 7;
        before.thread_priorities = {{1, 0x20u}};
        after = before;
        after.next_stack_top = 0x09FF0000u;               // the import changed these two fields
        after.thread_priorities = {{1, 0x20u}, {9, 0x30u}};
        current = before;
        current.next_uid = 8;                             // the decompiled run differs here: kept
        gt::KernelReplayState result = current;
        check(gt::apply_kernel_delta(result, before, after), "a delta without conflicts applies");
        check(result.next_stack_top == 0x09FF0000u && result.thread_priorities == after.thread_priorities,
              "fields and entries the import changed take its result");
        check(result.next_uid == 8, "fields it didn't change keep the current value");

        gt::KernelReplayState already = before;            // the host thread table kept the original's
        already.thread_priorities = {{1, 0x20u}, {9, 0x30u}};  // created thread: same value, no conflict
        check(gt::apply_kernel_delta(already, before, after) && already.thread_priorities == after.thread_priorities,
              "an added entry already present with the same value is no conflict");

        gt::KernelReplayState other = before;              // the decompiled run changed an entry the
        other.thread_priorities = {{1, 0x40u}};            // import didn't touch
        check(gt::apply_kernel_delta(other, before, after) && other.thread_priorities ==
                  std::vector<std::pair<std::int32_t, std::uint32_t>>({{1, 0x40u}, {9, 0x30u}}),
              "entries the import didn't touch keep the decompiled run's value (the final comparison sees it)");

        gt::KernelReplayState clash = before;
        clash.next_stack_top = 0x09000000u;                // neither the before nor the after value
        check(!gt::apply_kernel_delta(clash, before, after), "a field the import changed but that differs is a conflict");
        gt::KernelReplayState clash2 = before;
        clash2.thread_priorities = {{1, 0x20u}, {9, 0x31u}};  // the added entry with another value
        check(!gt::apply_kernel_delta(clash2, before, after), "an entry the import set but that differs is a conflict");
    }
    if (failures == 0) std::puts("import_replay: all tests passed");
    return failures == 0 ? 0 : 1;
}
