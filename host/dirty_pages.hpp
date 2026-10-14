// Which pages of a memory region are written while it is armed, and what they held before: the diff harness
// (host/diff_harness.cpp) uses it so a case copies, restores and compares only the RAM pages its runs wrote,
// not all 32 MiB. The region is write-protected; the first write to a page faults, the handler saves the
// page's bytes and lets the write through. One tracker is active at a time (the harness runs one case at a
// time); writes from any thread are seen. POSIX only (mprotect, SIGSEGV/SIGBUS).
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace gt {

class DirtyPages {
public:
    static constexpr std::size_t kPageSize = 4096u;

    DirtyPages() = default;
    ~DirtyPages();
    DirtyPages(const DirtyPages &) = delete;
    DirtyPages &operator=(const DirtyPages &) = delete;

    // Starts a case: the region's current bytes are its entry state; every page is write-protected.
    void begin(std::uint8_t *base, std::size_t size);
    // Pages written since begin() or the last rearm(), in the order they were first written.
    [[nodiscard]] std::vector<std::size_t> written() const;
    // Pages written since begin(), whatever rearm() did since.
    [[nodiscard]] std::vector<std::size_t> ever_written() const;
    // Writes the entry bytes back into every page written since begin(). The pages must be writable: call it
    // before rearm() or after disarm().
    void restore_pristine();
    // The entry state of the whole region into `destination` (size() bytes).
    void copy_pristine(std::uint8_t *destination) const;
    // The entry bytes of `page` (kPageSize bytes); only for pages written since begin().
    [[nodiscard]] const std::uint8_t *pristine_page(std::size_t page) const;
    // Protects every page again and starts a new written() list (a second run from the restored state).
    void rearm();
    // Every page saved and writable at once: for writes that don't fault (a system call writing into the
    // region fails with EFAULT instead). Costs one copy of the region.
    void materialize();
    // Writable again and no longer tracked; the lists and the pristine copy stay until end() or begin().
    void disarm();
    // disarm() and forget the case.
    void end();

    // Page index of an address inside the region; pages are counted from the region's first page boundary
    // at or below its start.
    [[nodiscard]] std::size_t page_of(const std::uint8_t *address) const;
    [[nodiscard]] std::size_t page_count() const noexcept { return pages_; }
    // The part of `page` inside the region: [*begin, *end) as offsets from the region's base.
    void page_span(std::size_t page, std::size_t &begin, std::size_t &end) const;
    [[nodiscard]] std::uint8_t *page_address(std::size_t page) const noexcept { return aligned_ + page * kPageSize; }
    [[nodiscard]] std::uint8_t *base() const noexcept { return base_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    // The fault handler's entry: true when `address` was this tracker's to handle.
    bool on_write_fault(const void *address) noexcept;

private:
    void protect_all(int protection) noexcept;

    std::uint8_t *base_{};
    std::size_t size_{};
    std::uint8_t *aligned_{};
    std::size_t pages_{};
    std::unique_ptr<std::uint8_t[]> pristine_;
    std::size_t pristine_pages_{};
    std::unique_ptr<std::atomic<std::uint8_t>[]> saved_;
    std::unique_ptr<std::atomic<std::uint8_t>[]> written_;
    std::unique_ptr<std::size_t[]> saved_list_;
    std::unique_ptr<std::size_t[]> written_list_;
    std::atomic<std::size_t> saved_count_{0u};
    std::atomic<std::size_t> written_count_{0u};
    bool armed_{};
};

} // namespace gt
