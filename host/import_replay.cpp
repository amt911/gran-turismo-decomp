#include "import_replay.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace gt {
namespace {

constexpr std::size_t kPage = 4096u;

std::string hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof text, "0x%08X", value);
    return text;
}

std::string describe(std::uint32_t stub, std::uint8_t mask, const std::array<std::uint32_t, 8> &args) {
    static constexpr const char *names[] = {"a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3"};
    std::string text = "stub " + hex(stub) + "(";
    bool first = true;
    for (unsigned i = 0u; i < args.size(); ++i) {
        if ((mask & (1u << i)) == 0u) continue;
        text += (first ? "" : " ") + std::string(names[i]) + "=" + hex(args[i]);
        first = false;
    }
    return text + ")";
}

bool same_masked(const std::array<std::uint32_t, 8> &a, const std::array<std::uint32_t, 8> &b, std::uint8_t mask) {
    for (unsigned i = 0u; i < a.size(); ++i)
        if ((mask & (1u << i)) != 0u && a[i] != b[i]) return false;
    return true;
}

} // namespace

std::vector<RamRun> ram_changes(std::span<const std::uint8_t> before, std::span<const std::uint8_t> after,
                                std::uint32_t base) {
    std::vector<RamRun> runs;
    const std::size_t size = std::min(before.size(), after.size());
    for (std::size_t page = 0u; page < size; page += kPage) {
        const std::size_t end = std::min(size, page + kPage);
        if (std::memcmp(before.data() + page, after.data() + page, end - page) == 0) continue;
        for (std::size_t i = page; i < end;) {
            if (before[i] == after[i]) {
                ++i;
                continue;
            }
            std::size_t j = i;
            while (j < end && before[j] != after[j]) ++j;
            // a run that reaches the page end continues in the next page's first bytes
            if (!runs.empty() && runs.back().address + runs.back().bytes.size() == base + i) {
                runs.back().bytes.insert(runs.back().bytes.end(), after.begin() + i, after.begin() + j);
            } else {
                runs.push_back({static_cast<std::uint32_t>(base + i), {after.begin() + i, after.begin() + j}});
            }
            i = j;
        }
    }
    return runs;
}

void apply_ram(std::span<std::uint8_t> ram, std::uint32_t base, const std::vector<RamRun> &runs) {
    for (const RamRun &run : runs) {
        if (run.address < base || run.address - base + run.bytes.size() > ram.size()) continue;
        std::copy(run.bytes.begin(), run.bytes.end(), ram.begin() + (run.address - base));
    }
}

RegisterChanges changed_registers(std::span<const std::uint32_t, 32> before, std::span<const std::uint32_t, 32> after) {
    RegisterChanges changes;
    for (unsigned r = 0u; r < 32u; ++r)
        if (before[r] != after[r]) changes.emplace_back(r, after[r]);
    return changes;
}

void apply_registers(std::span<std::uint32_t, 32> registers, const RegisterChanges &changes) {
    for (const auto &[index, value] : changes)
        if (index < 32u) registers[index] = value;
}

namespace {

// A plain field: taken when the import changed it and `current` still holds either end.
template <typename T>
bool merge_field(T &current, const T &before, const T &after) {
    if (before == after) return true;
    if (current != before && current != after) return false;
    current = after;
    return true;
}

// A uid -> value map stored as a vector of pairs: merged entry by entry.
template <typename K, typename V>
bool merge_map(std::vector<std::pair<K, V>> &current, const std::vector<std::pair<K, V>> &before,
               const std::vector<std::pair<K, V>> &after) {
    const auto find = [](const auto &map, const K &key) -> const V * {
        for (const auto &[k, v] : map)
            if (k == key) return &v;
        return nullptr;
    };
    const auto erase = [](auto &map, const K &key) {
        map.erase(std::remove_if(map.begin(), map.end(), [&](const auto &entry) { return entry.first == key; }),
                  map.end());
    };
    bool ok = true;
    for (const auto &[key, value] : after) {  // added or changed by the import
        const V *was = find(before, key);
        if (was != nullptr && *was == value) continue;
        const V *now = find(current, key);
        const bool consistent = now == nullptr ? was == nullptr : (was != nullptr && *now == *was) || *now == value;
        if (!consistent) ok = false;
        erase(current, key);
        current.emplace_back(key, value);
    }
    for (const auto &[key, value] : before) {  // removed by the import
        if (find(after, key) != nullptr) continue;
        const V *now = find(current, key);
        if (now != nullptr && !(*now == value)) ok = false;
        erase(current, key);
    }
    std::sort(current.begin(), current.end(), [](const auto &l, const auto &r) { return l.first < r.first; });
    return ok;
}

} // namespace

bool apply_kernel_delta(KernelReplayState &current, const KernelReplayState &before, const KernelReplayState &after) {
    bool ok = merge_map(current.blocks, before.blocks, after.blocks);
    ok = merge_field(current.free_ranges, before.free_ranges, after.free_ranges) && ok;
    ok = merge_field(current.next_address, before.next_address, after.next_address) && ok;
    ok = merge_field(current.next_stack_top, before.next_stack_top, after.next_stack_top) && ok;
    ok = merge_field(current.next_uid, before.next_uid, after.next_uid) && ok;
    ok = merge_map(current.thread_priorities, before.thread_priorities, after.thread_priorities) && ok;
    ok = merge_field(current.next_lwmutex_uid, before.next_lwmutex_uid, after.next_lwmutex_uid) && ok;
    ok = merge_map(current.mutexes, before.mutexes, after.mutexes) && ok;
    return ok;
}

void ImportLog::clear() noexcept {
    records_.clear();
    next_ = 0u;
}

void ImportLog::append(ImportRecord record) { records_.push_back(std::move(record)); }

ImportLog::Match ImportLog::next(std::uint32_t stub, const std::array<std::uint32_t, 8> &args) {
    const std::string number = std::to_string(next_ + 1u);
    if (next_ >= records_.size())
        return {nullptr, "import call " + number + ": " + describe(stub, 0u, args) +
                             " that the original didn't make (it made " + std::to_string(records_.size()) + ")"};
    const ImportRecord &record = records_[next_];
    if (record.stub != stub || !same_masked(args, record.args, record.arg_mask))
        return {nullptr, "import call " + number + ": expected " + describe(record.stub, record.arg_mask, record.args) +
                             ", got " + describe(stub, record.stub == stub ? record.arg_mask : std::uint8_t{0}, args)};
    ++next_;
    return {&record, {}};
}

std::optional<std::string> ImportLog::leftover() const {
    if (next_ >= records_.size()) return std::nullopt;
    const ImportRecord &record = records_[next_];
    return "the decompiled run made " + std::to_string(next_) + " of the original's " +
           std::to_string(records_.size()) + " import calls; the next was " +
           describe(record.stub, record.arg_mask, record.args);
}

} // namespace gt
