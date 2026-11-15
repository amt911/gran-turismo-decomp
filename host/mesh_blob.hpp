// GE "blobs": the static display-list fragments the game draws its models with through the leaf 0x08B550A8(blob)
// (docs/FACTS.md "Car meshes: GE blobs", docs/superpowers/specs/2026-11-07-crash-deformation-design.md). A blob is BASE, VADR, VTYPE,
// PRIM, RET followed by its own vertices; the vertex address in VADR is absolute, so a copy must rewrite it. Mod SDK v5 mesh accessors
// are thin wrappers over these functions.
#pragma once

#include "psprecomp/guest_memory.hpp"

#include <cstdint>
#include <vector>

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
// Known formats (FACTS): 0x122 stride 14 / position +8, 0x130, 0x112 and 0x11A stride 12 / position +6, 0x118 stride 8 / position +2 (GE vertex type:
// uv, color, normal, position, each aligned to its size; a morphing type such as 0x40112 stays unknown).
bool parse_blob(const psprecomp::GuestMemory &memory, std::uint32_t blob, BlobInfo &out);

// One run of vertices a blob draws: a VADR (or a VTYPE change inside one) starts it, each PRIM continues it. Blobs of the player's body have
// several (docs/FINDINGS.md "Crash deformation: the dent is not visible yet": 0x090EB4D0 has two VADRs, entry 22 two, ...). `parse_blob` refuses
// them; `parse_runs` lists every run of a known format (runs of another format, e.g. a morphing VTYPE, are skipped) so they can be dented in place.
struct Run {
    std::uint32_t vertex_data = 0;
    std::uint32_t count = 0;
    std::uint32_t stride = 0;
    std::uint32_t position_offset = 0;
    std::uint32_t normal_offset = 0;  // 3 x s8 normal at this offset inside a vertex (formats 0x122 and 0x130); 0 = the format has none
};
constexpr std::uint32_t kMaxRuns = 16u;
// True when the command list reaches its RET; `runs` then holds the runs (possibly none). Runs whose vertices are not in guest memory are skipped.
bool parse_runs(const psprecomp::GuestMemory &memory, std::uint32_t blob, std::vector<Run> &runs);
bool read_run_vertex(const psprecomp::GuestMemory &memory, const Run &run, std::uint32_t index, std::int16_t xyz[3]);
bool write_run_vertex(psprecomp::GuestMemory &memory, const Run &run, std::uint32_t index, const std::int16_t xyz[3]);
// The s8 normal of a vertex of a run that has one (false for a run without, or an index past the count).
bool read_run_normal(const psprecomp::GuestMemory &memory, const Run &run, std::uint32_t index, std::int8_t xyz[3]);
bool write_run_normal(psprecomp::GuestMemory &memory, const Run &run, std::uint32_t index, const std::int8_t xyz[3]);

bool read_vertex(const psprecomp::GuestMemory &memory, const BlobInfo &info, std::uint32_t index, std::int16_t xyz[3]);
bool write_vertex(psprecomp::GuestMemory &memory, const BlobInfo &info, std::uint32_t index, const std::int16_t xyz[3]);

// Copies the command list and the vertices to `destination` (copy_bytes(info) bytes) and points the copy's VADR (and the BASE before it) at
// the copy's own vertices. False when the source does not parse or the destination does not fit.
bool copy_blob(psprecomp::GuestMemory &memory, std::uint32_t blob, std::uint32_t destination);

} // namespace gt::mesh
