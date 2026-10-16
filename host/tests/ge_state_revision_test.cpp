// Unit tests for gt::GeStateRevision (host/ge_state_revision.cpp). Plain executable, run by CTest.
#include "ge_state_revision.hpp"

#include <cstdio>
#include <initializer_list>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main() {
    using gt::GeStateRevision;
    // per_draw_register
    for (unsigned c : {0x01u, 0x02u, 0x04u, 0x10u, 0x13u, 0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu, 0x0Fu, 0xC8u})
        check(GeStateRevision::per_draw_register(c), "per-draw register");
    for (unsigned c : {0x12u, 0x1Eu, 0x3Au, 0x3Bu, 0x9Cu, 0xA0u, 0xC2u, 0xC6u, 0xDEu, 0xEAu})
        check(!GeStateRevision::per_draw_register(c), "state register");

    GeStateRevision r;
    check(r.value() != 0u, "starts non-zero");
    const auto v0 = r.value();
    r.on_command(0xA0u, 5u, 5u);
    check(r.value() == v0, "same value does not bump");
    r.on_command(0xC8u, 1u, 2u);
    check(r.value() == v0, "texture level does not bump");
    r.on_command(0x01u, 1u, 2u);
    check(r.value() == v0, "vertex address does not bump");
    r.on_command(0xA0u, 5u, 6u);
    check(r.value() != v0, "texture address change bumps");
    const auto v1 = r.value();
    r.on_command(0x3Bu, 0u, 1u);
    check(r.value() != v1, "matrix data change bumps");

    // matrix_stream_repeated_value_bumps: matrix data registers fill the next element on each write, so a write of
    // the same value as the previous element can still change the matrix (review of #221).
    for (unsigned c : {0x2Bu, 0x3Bu, 0x3Du, 0x3Fu, 0x41u}) {
        const auto before = r.value();
        r.on_command(c, 7u, 7u);
        check(r.value() != before, "matrix data write with a repeated value bumps");
    }
    {
        const auto before = r.value();
        r.on_command(0x3Au, 0u, 0u);
        check(r.value() == before, "a matrix index register with the same value does not bump");
    }

    // transfer_and_list_start_bump
    const auto v2 = r.value();
    r.on_block_transfer();
    check(r.value() != v2, "block transfer bumps");
    const auto v3 = r.value();
    r.on_list_start();
    check(r.value() != v3, "list start bumps");
    check(r.value() != 0u, "never zero");
    return failures == 0 ? 0 : 1;
}
