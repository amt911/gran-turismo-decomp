// GE "blobs": the static display-list fragments the game draws its models with through the leaf 0x08B550A8(blob)
// (docs/FACTS.md "Car meshes: GE blobs", docs/superpowers/specs/2026-11-07-crash-deformation-design.md). A blob is BASE, VADR, VTYPE,
// PRIM, RET followed by its own vertices; the vertex address in VADR is absolute, so a copy must rewrite it. Mod SDK v5 mesh accessors
// are thin wrappers over these functions.
#pragma once

#include "psprecomp/guest_memory.hpp"

#include <cstdint>

namespace gt::mesh {

constexpr std::uint32_t kBlobHeaderBytes = 5u * 4u;  // BASE, VADR, VTYPE, PRIM, RET
constexpr std::uint32_t kMaxVertices = 0xFFFFu;

struct BlobInfo {
    std::uint32_t vertex_count = 0;
    std::uint32_t stride = 0;
    std::uint32_t position_offset = 0;  // the s16 x, y, z start here inside a vertex
    std::uint32_t vertex_data = 0;      // absolute address of vertex 0
    std::uint32_t byte_size = 0;        // header + vertices when the vertices follow the header, else the header only
};

// True when the five commands check out (BASE 0x10, VADR 0x01, VTYPE 0x12 of a known car format, PRIM 0x04 with a non-zero count, RET 0x0B)
// and the vertices lie in guest memory. Known formats (FACTS): 0x122 stride 14 / position +8, 0x130 and 0x112 stride 12 / position +6.
bool parse_blob(const psprecomp::GuestMemory &memory, std::uint32_t blob, BlobInfo &out);

bool read_vertex(const psprecomp::GuestMemory &memory, const BlobInfo &info, std::uint32_t index, std::int16_t xyz[3]);
bool write_vertex(psprecomp::GuestMemory &memory, const BlobInfo &info, std::uint32_t index, const std::int16_t xyz[3]);

// Copies a blob whose vertices follow its header to `destination` (byte_size bytes) and points the copy's VADR (and BASE) at the copy's own
// vertices. False when the source does not parse, the vertices do not follow the header, or the destination does not fit.
bool copy_blob(psprecomp::GuestMemory &memory, std::uint32_t blob, std::uint32_t destination);

// The translation of the latest world matrix recorded before `cursor` in a GE display list: the last WORLDN (0x3A) followed by 12 WORLDD
// (0x3B) words (a float's bits >> 8 each, 4 columns x 3 rows: the last three are the translation). Scans back at most `max_words`.
bool last_world_translation(const psprecomp::GuestMemory &memory, std::uint32_t cursor, std::uint32_t max_words, float xyz[3]);

} // namespace gt::mesh
