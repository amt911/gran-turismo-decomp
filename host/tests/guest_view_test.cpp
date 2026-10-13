// Unit tests for typed views of guest structs (include/gt/guest.hpp). A bare Runtime's guest memory;
// synthetic layouts. Plain executable, run by CTest.
#include "gt/guest.hpp"

#include "psprecomp/runtime.hpp"

#include <bit>
#include <cstdio>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

struct Inner : gt::guest::Layout {
    GT_FIELD(std::uint16_t, id, 0x2);
};

struct Sample : gt::guest::Layout {
    GT_FIELD(std::uint8_t, flag, 0x0);
    GT_FIELD(std::int8_t, offset, 0x1);
    GT_FIELD(std::uint16_t, half, 0x2);
    GT_FIELD(std::uint32_t, word, 0x4);
    GT_FIELD(float, ratio, 0x8);
    GT_FIELD(gt::guest::be16, big_half, 0xC);
    GT_FIELD(gt::guest::be32, big_word, 0x10);
    GT_ARRAY(std::uint8_t, bytes, 0x14, 1, 4);
    GT_ARRAY(std::uint32_t, words, 0x18, 8, 2);  // stride 8: one word every two
    GT_FIELD(Inner, inner, 0x20);
    GT_FIELD(gt::guest::Pointer<Inner>, next, 0x28);
};

} // namespace

int main() {
    psprecomp::Runtime rt;
    auto &memory = rt.memory();
    constexpr std::uint32_t base = 0x08900000u;
    gt::guest::View<Sample> sample(memory, base);

    sample.flag() = 0xABu;
    check(memory.load8(base) == 0xABu && sample.flag() == 0xABu, "u8 field stores and loads at +0");
    memory.store8(base + 1u, 0xFEu);
    check(sample.offset() == -2, "s8 field is signed");
    sample.half() = 0x1234u;
    check(memory.load16(base + 2u) == 0x1234u, "u16 field at +2");
    sample.word() = 0xDEADBEEFu;
    check(memory.load32(base + 4u) == 0xDEADBEEFu && sample.word() == 0xDEADBEEFu, "u32 field at +4");
    sample.ratio() = 1.5f;
    check(memory.load32(base + 8u) == std::bit_cast<std::uint32_t>(1.5f) && sample.ratio() == 1.5f,
          "float field keeps its bits");
    sample.big_half() = 0x1234u;
    check(memory.load16(base + 0xCu) == 0x3412u && sample.big_half() == 0x1234u, "be16 field is byte-swapped");
    sample.big_word() = 0x11223344u;
    check(memory.load32(base + 0x10u) == 0x44332211u && sample.big_word() == 0x11223344u,
          "be32 field is byte-swapped");
    sample.bytes(3) = 7u;
    check(memory.load8(base + 0x17u) == 7u && sample.bytes(3) == 7u, "array element i at offset + i * stride");
    sample.words(1) = 9u;
    check(memory.load32(base + 0x20u) == 9u, "array stride larger than the element");
    check(sample.inner().address() == base + 0x20u && sample.inner().id() == 0u, "nested struct is a view at its offset");
    sample.inner().id() = 0x55u;
    check(memory.load16(base + 0x22u) == 0x55u, "nested field stores through");
    sample.next() = base + 0x100u;
    check(memory.load32(base + 0x28u) == base + 0x100u, "pointer field stores the guest address");
    check(sample.next()->address() == base + 0x100u && static_cast<bool>(sample.next()), "pointer field follows to a view");
    sample.next() = 0u;
    check(!sample.next(), "a null pointer field is false");
    sample.word() += 1u;
    check(sample.word() == 0xDEADBEF0u, "compound assignment reads, modifies and writes");
    check(sample.address() == base, "a view knows its guest address");
    if (failures == 0) std::puts("guest_view: all tests passed");
    return failures == 0 ? 0 : 1;
}
