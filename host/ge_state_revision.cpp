#include "ge_state_revision.hpp"

namespace gt {

bool GeStateRevision::per_draw_register(std::uint32_t command) noexcept {
    switch (command) {
    case 0x01u: case 0x02u: case 0x04u:  // vertex / index address, PRIM
    case 0x10u: case 0x13u:              // BASE, OFFSET
    case 0x08u: case 0x09u: case 0x0Au: case 0x0Bu: case 0x0Cu: case 0x0Du: case 0x0Eu: case 0x0Fu:  // list flow
    case 0xC8u:  // texture level: recomputed per draw (patch 0039 carries the LOD in the vertices)
        return true;
    default:
        return false;
    }
}

bool GeStateRevision::matrix_stream_register(std::uint32_t command) noexcept {
    // Bone, world, view, projection and texture-generation matrix data: each write fills the next element, so the
    // previous write's value says nothing about the element it replaces.
    return command == 0x2Bu || command == 0x3Bu || command == 0x3Du || command == 0x3Fu || command == 0x41u;
}

void GeStateRevision::on_command(std::uint32_t command, std::uint32_t old_data, std::uint32_t data) noexcept {
    if (matrix_stream_register(command) || (old_data != data && !per_draw_register(command))) ++value_;
}

void GeStateRevision::on_list_start() noexcept { ++value_; }

void GeStateRevision::on_block_transfer() noexcept { ++value_; }

} // namespace gt
