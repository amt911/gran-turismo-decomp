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

std::string describe(std::uint32_t stub, unsigned arity, const std::array<std::uint32_t, 8> &args) {
    static constexpr const char *names[] = {"a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3"};
    std::string text = "stub " + hex(stub) + "(";
    for (unsigned i = 0u; i < arity && i < args.size(); ++i) text += (i ? " " : "") + std::string(names[i]) + "=" + hex(args[i]);
    return text + ")";
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
    const bool same_args = std::equal(args.begin(), args.begin() + std::min<std::size_t>(record.arity, 8u), record.args.begin());
    if (record.stub != stub || !same_args)
        return {nullptr, "import call " + number + ": expected " + describe(record.stub, record.arity, record.args) +
                             ", got " + describe(stub, record.stub == stub ? record.arity : 0u, args)};
    ++next_;
    return {&record, {}};
}

std::optional<std::string> ImportLog::leftover() const {
    if (next_ >= records_.size()) return std::nullopt;
    const ImportRecord &record = records_[next_];
    return "the decompiled run made " + std::to_string(next_) + " of the original's " +
           std::to_string(records_.size()) + " import calls; the next was " +
           describe(record.stub, record.arity, record.args);
}

} // namespace gt
