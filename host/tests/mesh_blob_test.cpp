// host/mesh_blob.hpp: GE "blob" parsing, vertex access and copy-on-write copies, against a guest image built here.
#include "mesh_blob.hpp"

#include <cstdio>
#include <memory>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
using namespace gt::mesh;
// A blob like the game's: BASE, VADR, VTYPE, PRIM, RET, then its vertices (docs/superpowers/specs/2026-11-07-crash-deformation-design.md).
constexpr std::uint32_t cmd(std::uint32_t op, std::uint32_t param) { return (op << 24) | (param & 0xFFFFFFu); }
std::uint32_t make_blob(psprecomp::GuestMemory &m, std::uint32_t at, std::uint32_t vtype, std::uint32_t count, std::uint32_t stride,
                        std::uint32_t pos_offset, const std::int16_t (*positions)[3]) {
    const std::uint32_t vertices = at + 5u * 4u;
    m.store32(at + 0, cmd(0x10, (vertices >> 8) & 0xFF0000u));
    m.store32(at + 4, cmd(0x01, vertices & 0xFFFFFFu));
    m.store32(at + 8, cmd(0x12, vtype));
    m.store32(at + 12, cmd(0x04, (3u << 16) | count));
    m.store32(at + 16, cmd(0x0B, 0));
    for (std::uint32_t i = 0; i < count; ++i)
        for (int c = 0; c < 3; ++c) m.store16(vertices + i * stride + pos_offset + 2u * c, static_cast<std::uint16_t>(positions[i][c]));
    return vertices;
}
} // namespace

int main() {
    auto owned = std::make_unique<psprecomp::GuestMemory>();
    psprecomp::GuestMemory &m = *owned;
    const std::int16_t pos[3][3] = {{100, 200, 300}, {-5, 6, -7}, {1, 2, 3}};
    constexpr std::uint32_t blob = 0x09200000u;
    make_blob(m, blob, 0x122, 3, 14, 8, pos);
    BlobInfo info{};
    check(parse_blob(m, blob, info) && info.vertex_count == 3 && info.stride == 14 && info.position_offset == 8 &&
              info.vertex_data == blob + 20u && info.byte_size == 20u + 3u * 14u,
          "blob: format 0x122 has stride 14 and the s16 position at +8");
    std::int16_t xyz[3];
    check(read_vertex(m, info, 1, xyz) && xyz[0] == -5 && xyz[1] == 6 && xyz[2] == -7, "blob: vertex read");
    check(write_vertex(m, info, 1, (const std::int16_t[3]){10, 20, 30}) && read_vertex(m, info, 1, xyz) && xyz[2] == 30,
          "blob: vertex write");
    check(!read_vertex(m, info, 3, xyz) && !write_vertex(m, info, 3, xyz), "blob: an index past the count is refused");
    constexpr std::uint32_t blob2 = 0x09210000u;
    make_blob(m, blob2, 0x130, 3, 12, 6, pos);
    BlobInfo info2{};
    check(parse_blob(m, blob2, info2) && info2.stride == 12 && info2.position_offset == 6, "blob: format 0x130 has stride 12 and position at +6");
    constexpr std::uint32_t blob3 = 0x09220000u;
    make_blob(m, blob3, 0x112, 3, 12, 6, pos);
    check(parse_blob(m, blob3, info2) && info2.stride == 12 && info2.position_offset == 6, "blob: format 0x112 has stride 12 and position at +6");
    constexpr std::uint32_t bad = 0x09230000u;
    make_blob(m, bad, 0x7FF, 3, 12, 6, pos);
    check(!parse_blob(m, bad, info2), "blob: an unknown vertex format is refused");
    m.store32(bad, cmd(0x55, 0));
    check(!parse_blob(m, bad, info2) && !parse_blob(m, 0u, info2), "blob: a wrong header or a null address is refused");
    // A copy: the VADR must point at the copy's own vertices, and editing the copy leaves the original alone.
    constexpr std::uint32_t copy = 0x09300000u;
    check(copy_blob(m, blob, copy), "copy: copies the blob");
    BlobInfo copied{};
    check(parse_blob(m, copy, copied) && copied.vertex_data == copy + 20u, "copy: the VADR is rewritten to the copy's vertices");
    check(write_vertex(m, copied, 0, (const std::int16_t[3]){0, 0, 0}) && read_vertex(m, info, 0, xyz) && xyz[0] == 100,
          "copy: a dent in the copy leaves the original");
    check(m.load32(copy + 0) == m.load32(blob + 0) && m.load32(copy + 16) == m.load32(blob + 16), "copy: the other commands are byte-identical");
    // The latest world matrix of a list: WORLDN then twelve WORLDD; the translation is the last three.
    constexpr std::uint32_t list = 0x09400000u;
    const auto put_matrix = [&](std::uint32_t at, float tx, float ty, float tz) {
        m.store32(at, cmd(0x3A, 0));
        const float elements[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, tx, ty, tz};
        for (int i = 0; i < 12; ++i) {
            std::uint32_t bits;
            __builtin_memcpy(&bits, &elements[i], 4);
            m.store32(at + 4u + 4u * i, cmd(0x3B, bits >> 8));
        }
    };
    put_matrix(list, 1.f, 2.f, 3.f);
    put_matrix(list + 52u, 10.f, 20.f, 30.f);
    m.store32(list + 104u, cmd(0x0A, 0x123456));  // a CALL after the matrices
    float origin[3] = {0, 0, 0};
    check(last_world_translation(m, list + 108u, 64, origin) && origin[0] == 10.f && origin[1] == 20.f && origin[2] == 30.f,
          "list: the latest world matrix's translation");
    check(!last_world_translation(m, list + 108u, 12, origin), "list: nothing found within too few words");
    check(!last_world_translation(m, 0x09500000u, 64, origin), "list: no matrix in empty memory");
    return failures == 0 ? 0 : 1;
}
