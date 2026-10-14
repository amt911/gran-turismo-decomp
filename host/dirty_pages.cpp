#include "dirty_pages.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include <sys/mman.h>

namespace gt {
namespace {

std::atomic<DirtyPages *> g_active{nullptr};
struct sigaction g_previous_segv {};
struct sigaction g_previous_bus {};

void forward(int signal, siginfo_t *info, void *context) {
    const struct sigaction &previous = signal == SIGSEGV ? g_previous_segv : g_previous_bus;
    if ((previous.sa_flags & SA_SIGINFO) != 0 && previous.sa_sigaction != nullptr) {
        previous.sa_sigaction(signal, info, context);
        return;
    }
    if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN && previous.sa_handler != nullptr) {
        previous.sa_handler(signal);
        return;
    }
    // Default action: restore it and return, so the faulting instruction runs again and the process dies
    // with the usual signal (and its core dump).
    std::signal(signal, SIG_DFL);
}

void on_fault(int signal, siginfo_t *info, void *context) {
    DirtyPages *tracker = g_active.load(std::memory_order_acquire);
    if (tracker != nullptr && tracker->on_write_fault(info->si_addr)) return;
    forward(signal, info, context);
}

void install_handler() {
    static const bool installed = [] {
        struct sigaction action {};
        action.sa_sigaction = on_fault;
        action.sa_flags = SA_SIGINFO | SA_NODEFER;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGSEGV, &action, &g_previous_segv) != 0 || sigaction(SIGBUS, &action, &g_previous_bus) != 0)
            throw std::runtime_error("dirty pages: cannot install the write-fault handler");
        return true;
    }();
    (void)installed;
}

} // namespace

DirtyPages::~DirtyPages() {
    end();
    DirtyPages *self = this;
    g_active.compare_exchange_strong(self, nullptr, std::memory_order_acq_rel);
}

void DirtyPages::begin(std::uint8_t *base, std::size_t size) {
    end();
    install_handler();
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    aligned_ = reinterpret_cast<std::uint8_t *>(address & ~static_cast<std::uintptr_t>(kPageSize - 1u));
    base_ = base;
    size_ = size;
    pages_ = (static_cast<std::size_t>(base + size - aligned_) + kPageSize - 1u) / kPageSize;
    if (pristine_pages_ != pages_) {
        pristine_ = std::make_unique<std::uint8_t[]>(pages_ * kPageSize);
        saved_ = std::make_unique<std::atomic<std::uint8_t>[]>(pages_);
        written_ = std::make_unique<std::atomic<std::uint8_t>[]>(pages_);
        saved_list_ = std::make_unique<std::size_t[]>(pages_);
        written_list_ = std::make_unique<std::size_t[]>(pages_);
        for (std::size_t p = 0; p < pages_; ++p) {
            saved_[p].store(0u, std::memory_order_relaxed);
            written_[p].store(0u, std::memory_order_relaxed);
        }
        pristine_pages_ = pages_;
    }
    saved_count_.store(0u);
    written_count_.store(0u);
    armed_.store(true, std::memory_order_release);
    g_active.store(this, std::memory_order_release);
    protect_all(PROT_READ);
}

bool DirtyPages::on_write_fault(const void *address) noexcept {
    const auto *byte = static_cast<const std::uint8_t *>(address);
    if (byte < aligned_ || byte >= aligned_ + pages_ * kPageSize) return false;
    // Disarmed meanwhile (disarm() makes the region writable before clearing armed_): the write just retries.
    if (!armed_.load(std::memory_order_acquire)) return true;
    const std::size_t page = static_cast<std::size_t>(byte - aligned_) / kPageSize;
    std::uint8_t *start = aligned_ + page * kPageSize;
    std::uint8_t expected = kUnsaved;
    if (saved_[page].compare_exchange_strong(expected, kSaving, std::memory_order_acq_rel)) {
        std::memcpy(pristine_.get() + page * kPageSize, start, kPageSize);
        saved_list_[saved_count_.fetch_add(1u, std::memory_order_acq_rel)] = page;
        saved_[page].store(kSaved, std::memory_order_release);
    } else {
        // Another thread is saving this page: it must not become writable before the copy is complete.
        while (saved_[page].load(std::memory_order_acquire) != kSaved) {
        }
    }
    if (written_[page].exchange(1u, std::memory_order_acq_rel) == 0u)
        written_list_[written_count_.fetch_add(1u, std::memory_order_acq_rel)] = page;
    if (mprotect(start, kPageSize, PROT_READ | PROT_WRITE) != 0) std::abort();  // the write would fault forever
    return true;
}

std::vector<std::size_t> DirtyPages::written() const {
    return {written_list_.get(), written_list_.get() + written_count_.load(std::memory_order_acquire)};
}

std::vector<std::size_t> DirtyPages::ever_written() const {
    return {saved_list_.get(), saved_list_.get() + saved_count_.load(std::memory_order_acquire)};
}

std::size_t DirtyPages::page_of(const std::uint8_t *address) const {
    return static_cast<std::size_t>(address - aligned_) / kPageSize;
}

void DirtyPages::page_span(std::size_t page, std::size_t &begin, std::size_t &end) const {
    const std::uint8_t *first = std::max(aligned_ + page * kPageSize, base_);
    const std::uint8_t *last = std::min(aligned_ + (page + 1u) * kPageSize, base_ + size_);
    begin = static_cast<std::size_t>(first - base_);
    end = last > first ? static_cast<std::size_t>(last - base_) : begin;
}

const std::uint8_t *DirtyPages::pristine_page(std::size_t page) const { return pristine_.get() + page * kPageSize; }

void DirtyPages::restore_pristine() {
    for (const std::size_t page : ever_written()) {
        std::size_t begin = 0, end = 0;
        page_span(page, begin, end);
        const std::size_t offset = static_cast<std::size_t>(base_ + begin - (aligned_ + page * kPageSize));
        std::memcpy(base_ + begin, pristine_page(page) + offset, end - begin);
    }
}

void DirtyPages::copy_pristine(std::uint8_t *destination) const {
    std::memcpy(destination, base_, size_);
    for (const std::size_t page : ever_written()) {
        std::size_t begin = 0, end = 0;
        page_span(page, begin, end);
        const std::size_t offset = static_cast<std::size_t>(base_ + begin - (aligned_ + page * kPageSize));
        std::memcpy(destination + begin, pristine_page(page) + offset, end - begin);
    }
}

void DirtyPages::rearm() {
    for (const std::size_t page : written()) written_[page].store(0u, std::memory_order_relaxed);
    written_count_.store(0u, std::memory_order_release);
    protect_all(PROT_READ);
}

void DirtyPages::materialize() {
    for (std::size_t page = 0; page < pages_; ++page) {
        std::uint8_t expected = kUnsaved;
        if (saved_[page].compare_exchange_strong(expected, kSaving, std::memory_order_acq_rel)) {
            std::memcpy(pristine_.get() + page * kPageSize, aligned_ + page * kPageSize, kPageSize);
            saved_list_[saved_count_.fetch_add(1u, std::memory_order_acq_rel)] = page;
            saved_[page].store(kSaved, std::memory_order_release);
        }
        if (written_[page].exchange(1u, std::memory_order_acq_rel) == 0u)
            written_list_[written_count_.fetch_add(1u, std::memory_order_acq_rel)] = page;
    }
    protect_all(PROT_READ | PROT_WRITE);
}

void DirtyPages::disarm() {
    if (!armed_.load(std::memory_order_acquire)) return;
    protect_all(PROT_READ | PROT_WRITE);  // first: a write already faulting then retries on a writable page
    armed_.store(false, std::memory_order_release);
}

void DirtyPages::end() {
    disarm();
    for (const std::size_t page : ever_written()) saved_[page].store(kUnsaved, std::memory_order_relaxed);
    for (const std::size_t page : written()) written_[page].store(0u, std::memory_order_relaxed);
    saved_count_.store(0u);
    written_count_.store(0u);
}

void DirtyPages::protect_all(int protection) noexcept {
    if (pages_ != 0u && mprotect(aligned_, pages_ * kPageSize, protection) != 0) {
        std::perror("dirty pages: mprotect");
        std::abort();
    }
}

} // namespace gt
