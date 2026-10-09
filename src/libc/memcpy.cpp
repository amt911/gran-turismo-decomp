// memcpy from the C library statically linked into GT's executable.
//
// Runtime 0x08B79A88 (ELF vaddr 0x375A88), 182 callers. Name evidence: behaviour — copies a2 bytes
// from a1 to a0 forwards and returns a0.
//
// The copy order is reproduced chunk by chunk, because it is observable when the buffers overlap:
//   - both pointers word-aligned: 16-byte blocks (all four words read, then written), then whole
//     words, then single bytes;
//   - otherwise: unaligned words (lwl/lwr then swl/swr), then single bytes. The count is signed in
//     this path (count / 4 words, count % 4 bytes).

#include "decomp_registry.hpp"

namespace gt::libc {

void memcpy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    auto &memory = rt.memory();
    const std::uint32_t dest = ctx.gpr[4];
    std::uint32_t to = dest;
    std::uint32_t from = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    if (((to | from) & 3u) == 0u) {
        for (std::uint32_t block = 0; block < (count >> 4u); ++block, to += 16u, from += 16u) {
            const std::uint32_t words[4]{memory.load32(from), memory.load32(from + 4u), memory.load32(from + 8u),
                                         memory.load32(from + 12u)};
            for (std::uint32_t i = 0; i < 4u; ++i) memory.store32(to + 4u * i, words[i]);
        }
        for (std::uint32_t word = 0; word < ((count & 0xFu) >> 2u); ++word, to += 4u, from += 4u)
            memory.store32(to, memory.load32(from));
        for (std::uint32_t i = 0; i < (count & 3u); ++i) memory.store8(to + i, memory.load8(from + i));
    } else {
        const auto signed_count = static_cast<std::int32_t>(count);
        for (std::int32_t word = 0; word < signed_count / 4; ++word, to += 4u, from += 4u) {
            std::uint8_t bytes[4];
            for (std::uint32_t i = 0; i < 4u; ++i) bytes[i] = memory.load8(from + i);
            for (std::uint32_t i = 0; i < 4u; ++i) memory.store8(to + i, bytes[i]);
        }
        for (std::int32_t i = 0; i < signed_count % 4; ++i)
            memory.store8(to + static_cast<std::uint32_t>(i), memory.load8(from + static_cast<std::uint32_t>(i)));
    }
    ctx.gpr[2] = dest;
}

} // namespace gt::libc

GT_DECOMP(0x08B79A88u, gt::libc::memcpy, gt::ReturnKind::Word);
