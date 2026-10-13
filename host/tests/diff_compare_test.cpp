// Unit tests for gt::first_difference (host/diff_compare.cpp). Plain executable, run by CTest.
#include "diff_compare.hpp"

#include <utility>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

gt::EndState base_state() {
    gt::EndState s;
    s.gpr[2] = 0x08901234u;   // v0
    s.gpr[29] = 0x09FFF000u;  // sp
    s.ram.assign(64u, 0xAAu);
    return s;
}

bool mentions(const std::optional<std::string> &d, const char *text) {
    return d.has_value() && d->find(text) != std::string::npos;
}

} // namespace

int main() {
    {
        const auto a = base_state();
        check(!gt::first_difference(a, a).has_value(), "identical states are equal");
    }
    {
        auto a = base_state(), b = base_state();
        b.gpr[2] = 0x08905678u;
        const auto d = gt::first_difference(a, b);
        check(mentions(d, "v0"), "v0 difference is reported by name");
        check(mentions(d, "08901234") && mentions(d, "08905678"), "both v0 values are reported");
    }
    {
        auto a = base_state(), b = base_state();
        b.gpr[8] = 0x1234u;  // t0: caller-saved
        check(!gt::first_difference(a, b).has_value(), "caller-saved temporaries are ignored");
    }
    {
        auto a = base_state(), b = base_state();
        b.gpr[16] = 1u;  // s0: callee-saved
        check(mentions(gt::first_difference(a, b), "s0"), "callee-saved s0 difference is reported");
    }
    {
        auto a = base_state(), b = base_state();
        b.ram[0x10] = 0x55u;
        const auto d = gt::first_difference(a, b);
        check(mentions(d, "08000010"), "first differing RAM byte is reported by guest address");
    }
    {
        auto a = base_state(), b = base_state();
        b.f0_bits = 0x3F800000u;
        check(mentions(gt::first_difference(a, b, gt::ReturnKind::Float), "f0"), "f0 counts for float returns");
        check(!gt::first_difference(a, b, gt::ReturnKind::Word).has_value(), "f0 is scratch for word returns");
    }
    {
        auto a = base_state(), b = base_state();
        b.gpr[3] = 0x77u;  // v1 used as a temporary, as GCC does
        check(!gt::first_difference(a, b, gt::ReturnKind::Word).has_value(), "v1 is scratch for word returns");
        check(mentions(gt::first_difference(a, b, gt::ReturnKind::DoubleWord), "v1"), "v1 counts for 64-bit returns");
    }
    {
        auto a = base_state(), b = base_state();
        b.gpr[2] = 0x1u;
        check(!gt::first_difference(a, b, gt::ReturnKind::Void).has_value(), "v0 is scratch for void functions");
    }
    {
        auto a = base_state(), b = base_state();
        a.ignore_begin = b.ignore_begin = 0x08000020u;
        a.ignore_end = b.ignore_end = 0x08000030u;
        a.ram[0x24] = 0x11u;  // original spilled a register into its (now dead) stack frame
        check(!gt::first_difference(a, b).has_value(), "writes inside the dead stack range are ignored");
        b.ram[0x30] = 0x22u;  // first byte after the range is live again
        check(mentions(gt::first_difference(a, b), "08000030"), "bytes right after the range still count");
    }
    {
        const auto list = gt::parse_address_list("0x08AD408C, 08ab94c4\n0x089FB918");
        check(list.has_value() && list->size() == 3u, "comma, space and newline separate addresses");
        check(list.has_value() && list->contains(0x08AB94C4u), "addresses without 0x are hex, any case");
        check(!gt::parse_address_list("0x08AD408C,banana").has_value(), "a bad token rejects the whole list");
        check(!gt::parse_address_list("0x108AD408C").has_value(), "more than 32 bits is rejected");
        const auto empty = gt::parse_address_list(" , ");
        check(empty.has_value() && empty->empty(), "separators alone give an empty list");
    }
    {
        const std::vector<std::pair<std::uint32_t, std::string>> unique{{0x08800000u, "a"}, {0x08800004u, "b"}};
        check(!gt::duplicate_registration(unique).has_value(), "distinct addresses: no duplicate");
        const std::vector<std::pair<std::uint32_t, std::string>> twice{
            {0x0885C38Cu, "gt::core::ptr_member_address"}, {0x08800004u, "b"},
            {0x0885C38Cu, "gt::game::race_operator::get_memory_assign_with_ghost"}};
        const auto message = gt::duplicate_registration(twice);
        check(message.has_value() && message->find("0x0885C38C") != std::string::npos &&
                  message->find("ptr_member_address") != std::string::npos &&
                  message->find("get_memory_assign_with_ghost") != std::string::npos,
              "an address registered twice is reported with both names");
    }
    if (failures == 0) std::puts("diff_compare_test: all passed");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
