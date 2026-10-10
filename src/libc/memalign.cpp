// memalign from the C library statically linked into GT's executable: the allocator core malloc
// calls (src/libc/malloc.cpp).
//
// Runtime 0x08B35840 (ELF vaddr 0x331840). Name evidence: malloc(size) is memalign(4, size); the
// function rounds the alignment up to a power of two (at least 4) and returns an address aligned to
// it, or NULL; ~1.2 million calls on the way to a race. Heap layout: src/libc/page_heap.hpp.

#include "decomp_registry.hpp"
#include "guest_call.hpp"
#include "page_heap.hpp"

#include <bit>

namespace gt::libc {
namespace {

using namespace page_heap;

constexpr std::uint32_t kBlockHeadAddr = 0x08C7A950u;  // sceKernelGetBlockHeadAddr(id, void **out) (import stub)
constexpr std::uint32_t kMemset = 0x08B36018u;
constexpr std::uint32_t kDescriptorBlockName = 0x08CB5518u;  // "PDI:MBLOCK"
constexpr std::uint32_t kPageBlockName = 0x08CB5524u;        // "PDI:ABLOCK"
constexpr std::uint32_t kHighBlockLimit = 0x08CE0DA4u;       // pages up to this size come from the top of memory

std::uint32_t align_up(std::uint32_t value, std::uint32_t mask) { return (value + mask) & ~mask; }

} // namespace

void memalign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    auto &memory = rt.memory();
    std::uint32_t align = ctx.gpr[4];
    std::uint32_t size = ctx.gpr[5];
    (void)call_guest(rt, ctx, kLock, {kLockObject});
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t page_base_out = frame.at(0x0u);
    const std::uint32_t table_out = frame.at(0x4u);  // also the descriptor block's address, first
    const std::uint32_t range = frame.at(0x8u);      // {page, first slot, end}

    // The request: alignment a power of two of at least 4 (16 for sizes that are multiples of 16);
    // a size below the alignment becomes the alignment, a larger one is rounded to min(alignment, 16).
    std::uint32_t rounded = size + 15u;
    if (size == 0u) {
        size = 1u;
        rounded = 16u;
    }
    std::uint32_t mask = align - 1u;
    if (align < 4u) {
        align = 4u;
        mask = 3u;
    }
    if ((align & mask) != 0u) {  // the next power of two (sllv: the shift is taken mod 32)
        const std::uint32_t shift = 32u - static_cast<std::uint32_t>(std::countl_zero(mask));
        align = 1u << (shift & 31u);
    }
    if (size == (rounded & ~0xFu) && align < 16u) align = 16u;
    if (size < align) size = align;
    else if (align < size) size = align_up(size, align < 16u ? align - 1u : 15u);

    // Room to align inside the slot when the slot's own alignment isn't enough.
    std::uint32_t cls = call_guest(rt, ctx, kSizeClass, {size});
    std::uint32_t extra = 0u;
    if (static_cast<std::int32_t>(cls) >= static_cast<std::int32_t>(kSmallClasses)) {
        if (align > 0x100u) extra = align - 0x100u;
    } else if (align > 0x100u) {
        extra = align - 0x100u;
    } else {
        const std::uint32_t slot_size = memory.load16(kSlotSizes + cls * 2u);
        if (slot_size != align_up(slot_size, align - 1u) && slot_size + 4u < size + align) extra = align - 4u;
    }
    if (extra != 0u) {
        size += extra;
        cls = call_guest(rt, ctx, kSizeClass, {size});
    }
    const bool small = static_cast<std::int32_t>(cls) < static_cast<std::int32_t>(kSmallClasses);
    const std::uint32_t pages_with_free = kHeap + kPagesWithFree + cls * 4u;

    std::uint32_t result = 0u;
    std::uint32_t page = memory.load32(pages_with_free);
    bool fresh = false;
    bool failed = false;
    if (page == 0u) {
        // No page with a free slot: make one, and first a descriptor block if none has spares.
        std::uint32_t descriptors = memory.load32(kHeap + kDescriptorBlocks);
        if (descriptors == 0u) {
            memory.store32(table_out, 0u);
            const auto id = static_cast<std::int32_t>(call_guest(rt, ctx, kAllocBlock, {kDescriptorBlockName, 1u, 0x1000u, 0u}));
            if (id <= 0) {
                failed = true;
            } else {
                (void)call_guest(rt, ctx, kBlockHeadAddr, {static_cast<std::uint32_t>(id), table_out});
                const std::uint32_t block = memory.load32(table_out);
                (void)call_guest(rt, ctx, kMemset, {block, 0u, 0xFF0u});
                memory.store32(block + kBlockSpare, block + kBlockDescriptors);
                memory.store32(block + kBlockId, static_cast<std::uint32_t>(id));
                memory.store32(block + kBlockSpareCount, kDescriptorsPerBlock);
                for (std::uint32_t i = 0u; i < kDescriptorsPerBlock; ++i) {  // owner, prev, next
                    const std::uint32_t descriptor = block + kBlockDescriptors + i * kDescriptorBytes;
                    memory.store32(descriptor, block);
                    memory.store32(descriptor + kPagePrev, descriptor - kDescriptorBytes);
                    memory.store32(descriptor + kPageNext, descriptor + kDescriptorBytes);
                }
                memory.store32(block + kBlockDescriptors + kPagePrev, 0u);
                memory.store32(block + kBlockDescriptors + (kDescriptorsPerBlock - 1u) * kDescriptorBytes + kPageNext, 0u);
                memory.store32(kHeap + kDescriptorBlocks, block);
                descriptors = block;
                if (block == 0u) failed = true;
            }
        }
        if (!failed) {
            memory.store32(page_base_out, 0u);
            // Page size: small classes get (pages so far + 2) KiB, at most 32 KiB, at least two slots
            // (else one slot of the request's size); a large request gets its own page.
            std::uint32_t slot_bytes = size;
            std::uint32_t page_bytes = size;
            if (small) {
                const std::uint32_t created = memory.load32(kHeap + kPageCount + cls * 4u) + 2u;
                page_bytes = created < 0x21u ? created << 10 : 0x8000u;
                slot_bytes = memory.load16(kSlotSizes + cls * 2u);
                if (page_bytes < slot_bytes * 2u) slot_bytes = size;
                if (page_bytes < slot_bytes) page_bytes = slot_bytes;
            }
            const std::uint32_t slots = page_bytes / slot_bytes;
            const std::uint32_t block_bytes = align_up(slot_bytes * slots, 0xFFu);
            const std::uint32_t high = block_bytes <= memory.load32(kHighBlockLimit) ? 1u : 0u;
            const auto id = static_cast<std::int32_t>(call_guest(rt, ctx, kAllocBlock, {kPageBlockName, high, block_bytes, 0u}));
            if (id <= 0) {
                failed = true;
            } else {
                (void)call_guest(rt, ctx, kBlockHeadAddr, {static_cast<std::uint32_t>(id), page_base_out});
                const std::uint32_t base = memory.load32(page_base_out);
                // Chain the slots into the free list.
                std::uint32_t slot = base;
                for (std::int32_t i = 0; i < static_cast<std::int32_t>(slots - 1u); ++i) {
                    memory.store32(slot, slot + slot_bytes);
                    slot += slot_bytes;
                }
                memory.store32(slot, 0u);

                // Take a spare descriptor.
                page = memory.load32(descriptors + kBlockSpare);
                const std::uint32_t spare = memory.load32(page + kPageNext);
                memory.store32(descriptors + kBlockSpare, spare);
                if (spare != 0u) memory.store32(spare + kPagePrev, 0u);
                memory.store32(page + kPagePrev, 0u);
                memory.store32(page + kPageNext, 0u);
                memory.store32(page + kPageBlock, static_cast<std::uint32_t>(id));
                memory.store32(page + kPageFreeList, base);
                memory.store16(page + kPageSlotBytes, static_cast<std::uint16_t>(small ? block_bytes : 0u));
                memory.store16(page + kPageFreeCount, static_cast<std::uint16_t>(slots));
                memory.store16(page + kPageClass, static_cast<std::uint16_t>(cls));
                memory.store16(page + kPageSlots, static_cast<std::uint16_t>(slots));
                memory.store32(pages_with_free, page);

                // A descriptor block with no spares left moves to the full list.
                const std::uint32_t left = memory.load32(descriptors + kBlockSpareCount) - 1u;
                memory.store32(descriptors + kBlockSpareCount, left);
                if (left == 0u) {
                    const std::uint32_t prev = memory.load32(descriptors + kBlockPrev);
                    const std::uint32_t next = memory.load32(descriptors + kBlockNext);
                    if (prev != 0u) memory.store32(prev + kBlockNext, next);
                    else memory.store32(kHeap + kDescriptorBlocks, next);
                    if (next != 0u) memory.store32(next + kBlockPrev, prev);
                    const std::uint32_t full = memory.load32(kHeap + kFullDescriptorBlocks);
                    memory.store32(kHeap + kFullDescriptorBlocks, descriptors);
                    memory.store32(descriptors + kBlockPrev, 0u);
                    memory.store32(descriptors + kBlockNext, full);
                    if (full != 0u) memory.store32(full + kBlockPrev, descriptors);
                }
                if (small) memory.store32(kHeap + kPageCount + cls * 4u, memory.load32(kHeap + kPageCount + cls * 4u) + 1u);
                fresh = true;
            }
        }
    }

    if (!failed) {
        // Take the page's first free slot; a new page is entered in the page table first.
        const std::uint32_t span = small ? memory.load16(page + kPageSlotBytes) : align_up(size, 0xFFu);
        const std::uint32_t slot = memory.load32(page + kPageFreeList);
        memory.store32(page + kPageFreeList, memory.load32(slot));
        if (fresh) {
            memory.store32(range + 0x8u, slot + span);
            memory.store32(range + 0x0u, page);
            memory.store32(range + 0x4u, slot);
            memory.store32(table_out, 0u);
            const std::uint32_t position = ~call_guest(rt, ctx, kFindPage, {table_out, kHeap, slot});
            if (call_guest(rt, ctx, kInsertPage, {kHeap, memory.load32(table_out), position, range}) == 0u) {
                (void)call_guest(rt, ctx, kReleasePage, {kHeap, cls, page});
                failed = true;
            }
        }
        if (!failed) {
            const auto free_count = static_cast<std::uint16_t>(memory.load16(page + kPageFreeCount) - 1u);
            memory.store16(page + kPageFreeCount, free_count);
            if (free_count == 0u) {
                // Last slot taken: the page moves to the class's full list.
                const std::uint32_t prev = memory.load32(page + kPagePrev);
                const std::uint32_t next = memory.load32(page + kPageNext);
                if (prev != 0u) memory.store32(prev + kPageNext, next);
                else memory.store32(pages_with_free, next);
                if (next != 0u) memory.store32(next + kPagePrev, prev);
                const std::uint32_t full = memory.load32(kHeap + kFullPages + cls * 4u);
                memory.store32(kHeap + kFullPages + cls * 4u, page);
                memory.store32(page + kPagePrev, 0u);
                memory.store32(page + kPageNext, full);
                if (full != 0u) memory.store32(full + kPagePrev, page);
            }
            result = align_up(slot, align - 1u);
        }
    }
    (void)call_guest(rt, ctx, kUnlock, {kLockObject});
    ctx.gpr[2] = result;
}

} // namespace gt::libc

GT_DECOMP(0x08B35840u, gt::libc::memalign, gt::ReturnKind::Word);
