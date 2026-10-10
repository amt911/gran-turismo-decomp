// free from the C library statically linked into GT's executable.
//
// Runtime 0x08B35254 (ELF vaddr 0x331254). Name evidence: behaviour — free(ptr): ignores NULL, and
// otherwise returns the block to the allocator that memalign (0x08B35840, malloc's core) carves it
// from, under the same heap mutex, and gives a page back once none of its slots is in use; ~1.1
// million calls on the way to a race. Heap layout: src/libc/page_heap.hpp.

#include "decomp_registry.hpp"
#include "guest_call.hpp"
#include "page_heap.hpp"

namespace gt::libc {

using namespace page_heap;

void free(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t ptr = ctx.gpr[4];
    if (ptr == 0u) return;
    auto &memory = rt.memory();
    (void)call_guest(rt, ctx, kLock, {kLockObject});
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t table_out = frame.at(0u);
    memory.store32(table_out, 0u);
    const auto index = static_cast<std::int32_t>(call_guest(rt, ctx, kFindPage, {table_out, kHeap, ptr}));
    if (index >= 0) {
        const std::uint32_t table = memory.load32(table_out);
        const std::uint32_t entry = table + static_cast<std::uint32_t>(index) * kEntryStride;
        const std::uint32_t page = memory.load32(entry + kEntryPage);
        const std::uint32_t base = memory.load32(entry + kEntryBase);
        const std::uint32_t cls = memory.load16(page + kPageClass);

        // The slot the pointer falls in (large classes hold one block per page).
        std::uint32_t slot = base;
        if (cls < kSmallClasses) {
            const std::uint32_t size = memory.load16(kSlotSizes + cls * 2u);
            const std::int32_t n = static_cast<std::int32_t>(ptr - base) / static_cast<std::int32_t>(size);
            slot = base + size * static_cast<std::uint32_t>(n);
        }
        memory.store32(slot, memory.load32(page + kPageFreeList));
        memory.store32(page + kPageFreeList, slot);
        const auto free_count = static_cast<std::uint16_t>(memory.load16(page + kPageFreeCount) + 1u);
        memory.store16(page + kPageFreeCount, free_count);

        if (free_count == 1u) {
            // First free slot: unlink the page from the full list ...
            const std::uint32_t prev = memory.load32(page + kPagePrev);
            const std::uint32_t next = memory.load32(page + kPageNext);
            if (prev != 0u) memory.store32(prev + kPageNext, next);
            else memory.store32(kHeap + kFullPages + cls * 4u, next);
            if (next != 0u) memory.store32(next + kPagePrev, prev);

            // ... and insert it into the class's pages with free slots, by descending address.
            const std::uint32_t head_slot = kHeap + kPagesWithFree + cls * 4u;
            std::uint32_t before = memory.load32(head_slot);
            std::uint32_t after = 0u;
            if (before == 0u || before < page) {
                after = before;
                before = 0u;
                memory.store32(head_slot, page);
            } else {
                for (after = memory.load32(before + kPageNext); after != 0u && !(after < page);
                     after = memory.load32(before + kPageNext)) {
                    before = after;
                }
                memory.store32(before + kPageNext, page);
            }
            memory.store32(page + kPagePrev, before);
            memory.store32(page + kPageNext, after);
            if (after != 0u) memory.store32(after + kPagePrev, page);
        }

        if (memory.load16(page + kPageFreeCount) == memory.load16(page + kPageSlots)) {
            (void)call_guest(rt, ctx, kReleasePage, {kHeap, cls, page});
            (void)call_guest(rt, ctx, kFreePage, {kHeap, memory.load32(table_out), static_cast<std::uint32_t>(index)});
        }
    }
    (void)call_guest(rt, ctx, kUnlock, {kLockObject});
}

} // namespace gt::libc

GT_DECOMP(0x08B35254u, gt::libc::free, gt::ReturnKind::Void);
