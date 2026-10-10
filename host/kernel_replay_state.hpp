// The host-side kernel state replay-safe imports may change (host/gt_kernel.cpp): the memory-block
// allocator (sceKernelAllocMemoryBlock & co), thread priorities (sceKernelChangeThreadPriority
// when it doesn't switch), kernel mutexes and the LwMutex uid counter. The diff harness saves it
// before a case, restores it between the original and the decompiled run, and requires both runs to
// leave it the same.
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace gt {

struct KernelReplayState {
    std::vector<std::pair<std::int32_t, std::pair<std::uint32_t, std::uint32_t>>> blocks;  // uid -> address, size
    std::vector<std::pair<std::uint32_t, std::uint32_t>> free_ranges;                      // address, size
    std::uint32_t next_address{};
    std::uint32_t next_stack_top{};  // high blocks and thread stacks share the top of user memory
    std::int32_t next_uid{};
    std::vector<std::pair<std::int32_t, std::uint32_t>> thread_priorities;  // uid -> priority
    std::int32_t next_lwmutex_uid{};  // sceKernelCreateLwMutex (its work area is guest RAM)
    std::vector<std::pair<std::int32_t, std::pair<std::int32_t, std::int32_t>>> mutexes;  // uid -> owner, count
    bool operator==(const KernelReplayState &) const = default;
};

KernelReplayState save_kernel_replay_state();
void restore_kernel_replay_state(const KernelReplayState &state);

} // namespace gt
