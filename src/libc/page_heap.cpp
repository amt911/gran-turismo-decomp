// The helpers of the C library's page heap (layout: src/libc/page_heap.hpp), called by memalign and
// free (src/libc/memalign.cpp, src/libc/free.cpp). Their names describe what those decompiled
// callers use them for; the originals' names are unknown.

#include "decomp_registry.hpp"
#include "guest_call.hpp"
#include "page_heap.hpp"

namespace gt::libc::page_heap {
namespace {

constexpr std::uint32_t kLockMutex = 0x08C7A8B0u;     // sceKernelLockMutex(id, count, timeout) (import stub)
constexpr std::uint32_t kUnlockMutex = 0x08C7A898u;   // sceKernelUnlockMutex(id, count) (import stub)
constexpr std::uint32_t kFreeBlock = 0x08C7A938u;     // frees a kernel block by id (import stub)
constexpr std::uint32_t kBlockHeadAddr = 0x08C7A950u; // (id, void **out) (import stub)
constexpr std::uint32_t kSuspendIntr = 0x08C7A888u;   // () -> state (import stub)
constexpr std::uint32_t kResumeIntr = 0x08C7A8A8u;    // (state) (import stub)
constexpr std::uint32_t kAllocBlockCall = 0x08B35598u; // (const args[4] *) -> block id
constexpr std::uint32_t kMemset = 0x08B36018u;
constexpr std::uint32_t kTableBlockName = 0x08CB550Cu; // "PDI:UBLOCK"

std::uint32_t entry_at(std::uint32_t table, std::uint32_t index) { return table + index * kEntryStride; }

void copy_entry(psprecomp::GuestMemory &memory, std::uint32_t to, std::uint32_t from) {
    memory.store32(to + kEntryBase, memory.load32(from + kEntryBase));
    memory.store32(to + kEntryEnd, memory.load32(from + kEntryEnd));
    memory.store32(to + kEntryPage, memory.load32(from + kEntryPage));
}

} // namespace

// heap_lock(mutex): sceKernelLockMutex(mutex, 1, NULL) — a jump to the import.
void heap_lock(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, kLockMutex, {ctx.gpr[4], 1u, 0u});
}

// heap_unlock(mutex): sceKernelUnlockMutex(mutex, 1).
void heap_unlock(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, kUnlockMutex, {ctx.gpr[4], 1u});
}

// size_class(size): sizes up to 0x68 in classes of 4 bytes; up to 0x7FF, the first class whose slot
// size (kSlotSizes) holds the request; 43 (a page of its own) beyond.
void size_class(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t size = ctx.gpr[4];
    std::uint32_t cls = kSmallClasses;
    if (size < 0x800u) {
        cls = (size - 1u) >> 2;
        if (size >= 0x69u) {
            std::int32_t low = 0x19;
            std::int32_t high = static_cast<std::int32_t>(kSmallClasses);
            std::int32_t mid = 0x22;
            do {
                if (rt.memory().load16(kSlotSizes + static_cast<std::uint32_t>(mid) * 2u) < size) low = mid + 1;
                else high = mid;
                mid = (low + high) >> 1;
            } while (low < high);
            cls = static_cast<std::uint32_t>(low);
        }
    }
    ctx.gpr[2] = cls;
}

// find_page(table **out, heap, ptr) -> the index of the entry whose [base, end) holds ptr, with *out
// its table; otherwise ~(where it would be inserted), or -1 with *out = NULL when that is in no
// table with room.
void find_page(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    auto &memory = rt.memory();
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t heap = ctx.gpr[5];
    const std::uint32_t ptr = ctx.gpr[6];
    std::uint32_t table = memory.load32(heap + kFirstTable);
    // The first table whose last entry ends above ptr.
    while (table != 0u) {
        const std::uint32_t last = memory.load32(table + kTableCount) - 1u;
        if (ptr < memory.load32(entry_at(table, last) + kEntryEnd)) {
            std::int32_t low = 0;
            auto high = static_cast<std::int32_t>(last);
            for (;;) {
                const std::int32_t mid = (low + high) >> 1;
                const std::uint32_t entry = entry_at(table, static_cast<std::uint32_t>(mid));
                const std::uint32_t base = memory.load32(entry + kEntryBase);
                const std::uint32_t end = memory.load32(entry + kEntryEnd);
                if (!(ptr < base) && ptr < end) {
                    memory.store32(out, table);
                    ctx.gpr[2] = static_cast<std::uint32_t>(mid);
                    return;
                }
                if (low < high) {
                    if (base < ptr) low = mid + 1;
                    else high = mid;
                    continue;
                }
                std::uint32_t result = ~static_cast<std::uint32_t>(mid);
                if (!(ptr < end)) {
                    result = ~static_cast<std::uint32_t>(mid + 1);
                    if (mid + 1 >= static_cast<std::int32_t>(kTableEntries)) {  // past a full table: the next one's start
                        table = memory.load32(table + kTableNext);
                        result = 0xFFFFFFFFu;
                    }
                }
                memory.store32(out, table);
                ctx.gpr[2] = result;
                return;
            }
        }
        table = memory.load32(table + kTableNext);
    }
    // Beyond every entry: the end of the last table, if it has room.
    table = memory.load32(heap + kLastTable);
    std::uint32_t result = 0xFFFFFFFFu;
    if (table != 0u) {
        const std::uint32_t count = memory.load32(table + kTableCount);
        result = ~count;
        if (count >= kTableEntries) {
            table = 0u;
            result = 0xFFFFFFFFu;
        }
    }
    memory.store32(out, table);
    ctx.gpr[2] = result;
}

// release_page(heap, class, page): frees the page's kernel block, unlinks the page from its class's
// list and returns its descriptor to its descriptor block — which goes back into the heap's list
// (by descending address) if it was full, or is freed too once all 145 descriptors are spare.
void release_page(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    auto &memory = rt.memory();
    const std::uint32_t heap = ctx.gpr[4];
    const std::uint32_t cls = ctx.gpr[5];
    const std::uint32_t page = ctx.gpr[6];
    const std::uint32_t block = memory.load32(page);
    if (static_cast<std::int32_t>(cls) < static_cast<std::int32_t>(kSmallClasses)) {
        memory.store32(kHeap + kPageCount + cls * 4u, memory.load32(kHeap + kPageCount + cls * 4u) - 1u);
    }
    (void)call_guest(rt, ctx, kFreeBlock, {memory.load32(page + kPageBlock)});
    const std::uint32_t prev = memory.load32(page + kPagePrev);
    memory.store32(page + kPageBlock, 0u);
    memory.store32(page + kPageFreeList, 0u);
    const std::uint32_t next = memory.load32(page + kPageNext);
    if (prev != 0u) memory.store32(prev + kPageNext, next);
    else memory.store32(heap + kPagesWithFree + cls * 4u, next);
    if (next != 0u) memory.store32(next + kPagePrev, prev);

    const std::uint32_t spare = memory.load32(block + kBlockSpare);
    memory.store32(block + kBlockSpare, page);
    memory.store32(page + kPagePrev, 0u);
    memory.store32(page + kPageNext, spare);
    if (spare != 0u) memory.store32(spare + kPagePrev, page);

    const std::uint32_t spares = memory.load32(block + kBlockSpareCount);
    memory.store32(block + kBlockSpareCount, spares + 1u);
    if (spares == 0u) {
        // From the full list back into the list of blocks with spares, by descending address.
        const std::uint32_t full_prev = memory.load32(block + kBlockPrev);
        const std::uint32_t full_next = memory.load32(block + kBlockNext);
        if (full_prev != 0u) memory.store32(full_prev + kBlockNext, full_next);
        else memory.store32(heap + kFullDescriptorBlocks, full_next);
        if (full_next != 0u) memory.store32(full_next + kBlockPrev, full_prev);
        std::uint32_t before = memory.load32(heap + kDescriptorBlocks);
        std::uint32_t after = 0u;
        if (before == 0u || before < block) {
            after = before;
            before = 0u;
            memory.store32(heap + kDescriptorBlocks, block);
        } else {
            for (after = memory.load32(before + kBlockNext); after != 0u && !(after < block);
                 after = memory.load32(before + kBlockNext)) {
                before = after;
            }
            memory.store32(before + kBlockNext, block);
        }
        memory.store32(block + kBlockPrev, before);
        memory.store32(block + kBlockNext, after);
        if (after != 0u) memory.store32(after + kBlockPrev, block);
    } else if (spares + 1u == kDescriptorsPerBlock) {
        // Every descriptor spare: unlink the block and free it (a jump to the import).
        const std::uint32_t block_prev = memory.load32(block + kBlockPrev);
        const std::uint32_t block_next = memory.load32(block + kBlockNext);
        if (block_prev != 0u) memory.store32(block_prev + kBlockNext, block_next);
        else memory.store32(heap + kDescriptorBlocks, block_next);
        if (block_next != 0u) memory.store32(block_next + kBlockPrev, block_prev);
        tail_call(ctx, kFreeBlock, {memory.load32(block + kBlockId)});
    }
}

// remove_page_entry(heap, table, index): removes the entry, pulling every later table's first entry
// back into the table before it (all tables but the last stay full); an emptied last table is
// unlinked and its block freed.
void remove_page_entry(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    auto &memory = rt.memory();
    const std::uint32_t heap = ctx.gpr[4];
    std::uint32_t table = ctx.gpr[5];
    std::uint32_t index = ctx.gpr[6];
    std::uint32_t count = 0u;
    for (;;) {
        count = memory.load32(table + kTableCount);
        const auto later = static_cast<std::int32_t>(count - index - 1u);
        for (std::int32_t i = 0; i < later; ++i) {
            const std::uint32_t entry = entry_at(table, index + static_cast<std::uint32_t>(i));
            copy_entry(memory, entry, entry + kEntryStride);
        }
        const std::uint32_t next = memory.load32(table + kTableNext);
        index = 0u;
        if (next == 0u) break;
        copy_entry(memory, entry_at(table, kTableEntries - 1u), entry_at(next, 0u));
        table = next;
    }
    memory.store32(table + kTableCount, count - 1u);
    if (count - 1u != 0u) return;
    const std::uint32_t prev = memory.load32(table + kTablePrev);
    if (prev != 0u) memory.store32(prev + kTableNext, 0u);
    else memory.store32(heap + kFirstTable, 0u);
    memory.store32(heap + kLastTable, prev);
    tail_call(ctx, kFreeBlock, {memory.load32(table + kTableBlock)});
}

// insert_page(heap, table, position, const entry *) -> 1, or 0 when a kernel block can't be had.
// With no table, a new "PDI:UBLOCK" table is appended; a full table first pushes its last entry into
// the next table.
void insert_page(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    auto &memory = rt.memory();
    const std::uint32_t heap = ctx.gpr[4];
    std::uint32_t table = ctx.gpr[5];
    const std::uint32_t position = ctx.gpr[6];
    const std::uint32_t entry = ctx.gpr[7];
    ctx.gpr[2] = 0u;
    if (table == 0u) {
        const auto id = static_cast<std::int32_t>(call_guest(rt, ctx, kAllocBlock, {kTableBlockName, 1u, 0x1000u, 0u}));
        if (id <= 0) return;
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t head = frame.at(0u);
        memory.store32(head, 0u);
        (void)call_guest(rt, ctx, kBlockHeadAddr, {static_cast<std::uint32_t>(id), head});
        (void)call_guest(rt, ctx, kMemset, {memory.load32(head), 0u, 0x1000u});
        memory.store32(memory.load32(head) + kTableBlock, static_cast<std::uint32_t>(id));
        table = memory.load32(head);
        if (table == 0u) return;
        const std::uint32_t last = memory.load32(heap + kLastTable);
        memory.store32(table + kTablePrev, last);
        if (last != 0u) memory.store32(last + kTableNext, table);
        else memory.store32(heap + kFirstTable, table);
        memory.store32(heap + kLastTable, table);
    }
    const std::uint32_t count = memory.load32(table + kTableCount);
    auto later = static_cast<std::int32_t>(count - position);
    if (count == kTableEntries) {
        const std::uint32_t pushed = call_guest(rt, ctx, kInsertPage, {heap, memory.load32(table + kTableNext), 0u,
                                                                       entry_at(table, kTableEntries - 1u) + kEntryPage});
        --later;
        if (pushed == 0u) return;
    } else {
        memory.store32(table + kTableCount, count + 1u);
    }
    for (std::int32_t i = later; i > 0; --i) {
        const std::uint32_t to = entry_at(table, position + static_cast<std::uint32_t>(i));
        copy_entry(memory, to, to - kEntryStride);
    }
    const std::uint32_t slot = entry_at(table, position);
    memory.store32(slot + kEntryEnd, memory.load32(entry + 0x8u));
    memory.store32(slot + kEntryPage, memory.load32(entry + 0x0u));
    memory.store32(slot + kEntryBase, memory.load32(entry + 0x4u));
    ctx.gpr[2] = 1u;
}

// alloc_block(name, type, size, addr) -> kernel block id: the four arguments as a struct, passed to
// the allocating call with interrupts suspended.
void alloc_block(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    auto &memory = rt.memory();
    const std::uint32_t state = call_guest(rt, ctx, kSuspendIntr, {});
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t args = frame.at(0u);
    for (std::uint32_t i = 0u; i < 4u; ++i) memory.store32(args + i * 4u, ctx.gpr[4u + i]);
    const std::uint32_t id = call_guest(rt, ctx, kAllocBlockCall, {args});
    (void)call_guest(rt, ctx, kResumeIntr, {state});
    ctx.gpr[2] = id;
}

} // namespace gt::libc::page_heap

GT_DECOMP(0x08B344E0u, gt::libc::page_heap::heap_lock, gt::ReturnKind::Word);
GT_DECOMP(0x08B3457Cu, gt::libc::page_heap::heap_unlock, gt::ReturnKind::Word);
GT_DECOMP(0x08B34DE0u, gt::libc::page_heap::size_class, gt::ReturnKind::Word);
GT_DECOMP(0x08B34FD4u, gt::libc::page_heap::find_page, gt::ReturnKind::Word);
GT_DECOMP(0x08B34E44u, gt::libc::page_heap::release_page, gt::ReturnKind::Void);
GT_DECOMP(0x08B350C8u, gt::libc::page_heap::remove_page_entry, gt::ReturnKind::Void);
GT_DECOMP(0x08B356C8u, gt::libc::page_heap::insert_page, gt::ReturnKind::Word);
GT_DECOMP(0x08B3564Cu, gt::libc::page_heap::alloc_block, gt::ReturnKind::Word);
