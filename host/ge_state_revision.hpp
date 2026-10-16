#pragma once

#include <cstdint>

namespace gt {

// Draw-state revision for the GE list executor: bumped when a register that a draw's state depends on changes
// value; per-draw registers (addresses, PRIM, list flow, the texture level 0xC8) never bump it. The renderer's
// descriptor, lighting and transform caches key on it (docs/superpowers/specs/2026-10-16-ge-fast-path-design.md).
class GeStateRevision {
public:
    static bool per_draw_register(std::uint32_t command) noexcept;
    // Call before storing `data` in `commands[command]`.
    void on_command(std::uint32_t command, std::uint32_t old_data, std::uint32_t data) noexcept;
    void on_list_start() noexcept;      // new list: hashes and remembered content are forgotten
    void on_block_transfer() noexcept;  // 0xEA rewrote guest memory
    std::uint64_t value() const noexcept { return value_; }

private:
    std::uint64_t value_{1u};  // never 0: 0 means "no revision" to render_ge_primitive
};

} // namespace gt
