// The C library's heap behind malloc/memalign/free (src/libc/memalign.cpp, src/libc/free.cpp).
//
// Not dlmalloc: a size-class page allocator. Small requests (size classes below 43) share pages of
// equal slots; larger ones get a page each. Pages come from the kernel's block allocator ("PDI:ABLOCK"
// blocks), their descriptors from "PDI:MBLOCK" blocks of 145. Field names describe what the decompiled
// functions do with each field; the allocator's own names are unknown.
#pragma once

#include <cstdint>

namespace gt::libc::page_heap {

constexpr std::uint32_t kLockObject = 0x08D69278u;  // the heap's mutex object
constexpr std::uint32_t kHeap = 0x08D692A0u;        // the heap state
constexpr std::uint32_t kLock = 0x08B344E0u;        // (mutex *)
constexpr std::uint32_t kUnlock = 0x08B3457Cu;      // (mutex *)
constexpr std::uint32_t kSizeClass = 0x08B34DE0u;   // (size) -> size class
constexpr std::uint32_t kFindPage = 0x08B34FD4u;    // (table **out, heap *, ptr) -> index in *out; ~insertion point if absent
constexpr std::uint32_t kInsertPage = 0x08B356C8u;  // (heap *, table *, position, page range *) -> 0 on failure
constexpr std::uint32_t kReleasePage = 0x08B34E44u; // (heap *, class, page)
constexpr std::uint32_t kFreePage = 0x08B350C8u;    // (heap *, table *, index)
constexpr std::uint32_t kSlotSizes = 0x08C93B4Cu;   // u16[43]: slot size of each small size class
constexpr std::uint32_t kSmallClasses = 43u;

// Heap state.
constexpr std::uint32_t kDescriptorBlocks = 0x00u;      // descriptor blocks with spare descriptors
constexpr std::uint32_t kFullDescriptorBlocks = 0x04u;  // descriptor blocks with none left
constexpr std::uint32_t kPagesWithFree = 0x08u;         // [class]: pages with free slots, by descending address
constexpr std::uint32_t kFullPages = 0xB8u;             // [class]: pages with no free slot
constexpr std::uint32_t kPageCount = 0x170u;            // [class]: pages created (sizes the next page)

// Page table entry (12 bytes, from +0x10 of the table kFindPage returns): page descriptor, first slot.
constexpr std::uint32_t kEntryStride = 12u;
constexpr std::uint32_t kEntryPage = 0x10u;
constexpr std::uint32_t kEntryBase = 0x14u;

// Page descriptor (0x1C bytes).
constexpr std::uint32_t kPagePrev = 0x04u;
constexpr std::uint32_t kPageNext = 0x08u;
constexpr std::uint32_t kPageBlock = 0x0Cu;      // its kernel block id
constexpr std::uint32_t kPageFreeList = 0x10u;
constexpr std::uint32_t kPageClass = 0x14u;      // u16
constexpr std::uint32_t kPageSlotBytes = 0x16u;  // u16: the page's byte size (small classes; 0 otherwise)
constexpr std::uint32_t kPageSlots = 0x18u;      // u16: slots in the page
constexpr std::uint32_t kPageFreeCount = 0x1Au;  // u16

// Descriptor block (one 0x1000-byte kernel block): list links, the next spare descriptor, its block
// id, how many spare descriptors are left, then 145 descriptors from +0x14.
constexpr std::uint32_t kBlockNext = 0x00u;
constexpr std::uint32_t kBlockPrev = 0x04u;
constexpr std::uint32_t kBlockSpare = 0x08u;
constexpr std::uint32_t kBlockId = 0x0Cu;
constexpr std::uint32_t kBlockSpareCount = 0x10u;
constexpr std::uint32_t kBlockDescriptors = 0x14u;
constexpr std::uint32_t kDescriptorsPerBlock = 0x91u;
constexpr std::uint32_t kDescriptorBytes = 0x1Cu;

} // namespace gt::libc::page_heap
