// GE "blobs": the static display-list fragments the game draws its models with through the leaf 0x08B550A8(blob)
// (docs/FACTS.md "Car meshes: GE blobs", docs/superpowers/specs/2026-11-07-crash-deformation-design.md). A blob is BASE, VADR, VTYPE,
// PRIM, RET followed by its own vertices; the vertex address in VADR is absolute, so a copy must rewrite it. Mod SDK v5 mesh accessors
// are thin wrappers over these functions.
#pragma once

#include "psprecomp/guest_memory.hpp"

#include <cstdint>

namespace gt::mesh {

constexpr std::uint32_t kMaxCommandWords = 8192u;

struct BlobInfo {
    std::uint32_t vertex_count = 0;    // the sum of the PRIM counts: the vertices are read in order, each PRIM continuing the last
    std::uint32_t stride = 0;
    std::uint32_t position_offset = 0;  // the s16 x, y, z start here inside a vertex
    std::uint32_t vertex_data = 0;      // absolute address of vertex 0
    std::uint32_t command_bytes = 0;    // from the blob's first word to its RET (inclusive)
};

// What mesh_copy / copy_blob need in memory: the command list, then the vertices (4-byte aligned).
inline std::uint32_t copy_bytes(const BlobInfo &info) { return info.command_bytes + info.vertex_count * info.stride; }

// Scans the command list up to its RET (0x0B): one VADR 0x01 (the vertex array, absolute through the BASE 0x10 before it), a VTYPE 0x12 of a
// known car format and at least one PRIM 0x04; other commands (ATEST, CALLs of texture blocks, ...) are skipped. The real blobs look like
// `ATEST ... BASE VADR ... BASE CALL(texture) 0xC8 VTYPE(0x112) PRIM 0xC8 PRIM ... RET` (docs/FINDINGS.md "GE blob census", measured
// with the deformation probe 2026-11-14). False for a second, different VADR, an unknown format or vertices outside guest memory.
// Known formats (FACTS): 0x122 stride 14 / position +8, 0x130 and 0x112 stride 12 / position +6.
bool parse_blob(const psprecomp::GuestMemory &memory, std::uint32_t blob, BlobInfo &out);

bool read_vertex(const psprecomp::GuestMemory &memory, const BlobInfo &info, std::uint32_t index, std::int16_t xyz[3]);
bool write_vertex(psprecomp::GuestMemory &memory, const BlobInfo &info, std::uint32_t index, const std::int16_t xyz[3]);

// Copies the command list and the vertices to `destination` (copy_bytes(info) bytes) and points the copy's VADR (and the BASE before it) at
// the copy's own vertices. False when the source does not parse or the destination does not fit.
bool copy_blob(psprecomp::GuestMemory &memory, std::uint32_t blob, std::uint32_t destination);

} // namespace gt::mesh
