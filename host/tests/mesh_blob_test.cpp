// host/mesh_blob.hpp: GE "blob" parsing, vertex access and copy-on-write copies, against a guest image built here.
#include "mesh_blob.hpp"

#include <cstdio>
#include <memory>
#include <vector>

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
              info.vertex_data == blob + 20u && info.command_bytes == 20u,
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
    // Runs: two VADRs (the player's body parts have them), a format change inside one VADR, and a run of an unknown format that is skipped.
    {
        constexpr std::uint32_t multi = 0x09240000u, va = 0x09241000u, vb = 0x09242000u, vc = 0x09243000u;
        std::uint32_t at = multi;
        const auto put = [&](std::uint32_t op, std::uint32_t param) { m.store32(at, cmd(op, param)); at += 4u; };
        put(0x10, (va >> 8) & 0xFF0000u); put(0x01, va & 0xFFFFFFu); put(0x12, 0x130); put(0x04, 3); put(0x04, 2);
        put(0x12, 0x122); put(0x04, 4);                        // same VADR, the format changes: a second run right after the 5 vertices
        put(0x01, vb & 0xFFFFFFu); put(0x10, (vb >> 8) & 0xFF0000u); put(0x01, vb & 0xFFFFFFu); put(0x12, 0x118); put(0x04, 2);  // unknown format
        put(0x10, (vc >> 8) & 0xFF0000u); put(0x01, vc & 0xFFFFFFu); put(0x12, 0x112); put(0x04, 1); put(0x0B, 0);
        for (int i = 0; i < 5; ++i) for (int c = 0; c < 3; ++c) m.store16(va + 12u * i + 6u + 2u * c, static_cast<std::uint16_t>(100 * i + c));
        for (int i = 0; i < 4; ++i) for (int c = 0; c < 3; ++c) m.store16(va + 60u + 14u * i + 8u + 2u * c, static_cast<std::uint16_t>(-(100 * i + c)));
        std::vector<Run> runs;
        check(parse_runs(m, multi, runs) && runs.size() == 3, "runs: 0x130 x5, 0x122 x4 after them, the 0x118 run skipped, 0x112 x1");
        check(runs.size() == 3 && runs[0].vertex_data == va && runs[0].count == 5 && runs[0].stride == 12 && runs[1].vertex_data == va + 60u &&
                  runs[1].count == 4 && runs[1].stride == 14 && runs[1].position_offset == 8 && runs[2].vertex_data == vc && runs[2].count == 1,
              "runs: addresses, counts and strides");
        std::int16_t v[3];
        check(runs.size() == 3 && read_run_vertex(m, runs[0], 4, v) && v[0] == 400 && v[2] == 402 && read_run_vertex(m, runs[1], 3, v) && v[0] == -300,
              "runs: vertex read of the first and the second run");
        check(runs.size() == 3 && write_run_vertex(m, runs[1], 3, (const std::int16_t[3]){7, 8, 9}) && read_run_vertex(m, runs[1], 3, v) && v[1] == 8 &&
                  !write_run_vertex(m, runs[1], 4, v),
              "runs: vertex write, an index past the count refused");
        check(!parse_blob(m, multi, info2), "runs: parse_blob still refuses a blob with several VADRs");
        std::vector<Run> none;
        check(!parse_runs(m, 0u, none) && none.empty(), "runs: a null blob is refused");
    }
    // A copy: the VADR must point at the copy's own vertices, and editing the copy leaves the original alone.
    constexpr std::uint32_t copy = 0x09300000u;
    check(copy_blob(m, blob, copy), "copy: copies the blob");
    BlobInfo copied{};
    check(parse_blob(m, copy, copied) && copied.vertex_data == copy + 20u, "copy: the VADR is rewritten to the copy's vertices");
    check(write_vertex(m, copied, 0, (const std::int16_t[3]){0, 0, 0}) && read_vertex(m, info, 0, xyz) && xyz[0] == 100,
          "copy: a dent in the copy leaves the original");
    check(m.load32(copy + 0) == m.load32(blob + 0) && m.load32(copy + 16) == m.load32(blob + 16), "copy: the other commands are byte-identical");
    // A blob like the real ones (measured 2026-11-14): ATEST, BASE+VADR of the vertices, BASE+CALL of a texture block, then VTYPE and
    // several PRIMs, RET, a padding word and the vertices.
    {
        constexpr std::uint32_t real = 0x09A00000u;
        const std::uint32_t words[] = {cmd(0x22, 1), cmd(0x17, 0), cmd(0x1E, 1), cmd(0x10, 0x090000), cmd(0x01, 0xA00000u + 15u * 4u + 4u),
                                       cmd(0x21, 0), cmd(0x10, 0x090000), cmd(0x0A, 0x9CD40C), cmd(0xC8, 0xFA0000), cmd(0x12, 0x112),
                                       cmd(0x04, 0x040002), cmd(0xC8, 0xF90000), cmd(0x04, 0x040003), cmd(0xC8, 0xF00000), cmd(0x0B, 0)};
        for (unsigned i = 0; i < 15; ++i) m.store32(real + 4u * i, words[i]);
        const std::uint32_t vertices = real + 15u * 4u + 4u;  // one padding word after RET
        for (std::uint32_t v = 0; v < 5; ++v)
            for (int c = 0; c < 3; ++c) m.store16(vertices + v * 12u + 6u + 2u * c, static_cast<std::uint16_t>(100 * v + c));
        BlobInfo r{};
        check(parse_blob(m, real, r) && r.vertex_count == 5 && r.stride == 12 && r.vertex_data == vertices && r.command_bytes == 60u,
              "real blob: PRIM counts add up, the vertices start at the VADR after a padding word");
        std::int16_t v[3];
        check(read_vertex(m, r, 4, v) && v[0] == 400 && v[2] == 402, "real blob: vertex read");
        constexpr std::uint32_t copy2 = 0x09B00000u;
        check(copy_blob(m, real, copy2), "real blob: copy");
        BlobInfo c2{};
        check(parse_blob(m, copy2, c2) && c2.vertex_data == copy2 + 60u && c2.vertex_count == 5, "real blob: the copy's VADR names its own vertices (padding dropped)");
        check(m.load32(copy2 + 7u * 4u) == m.load32(real + 7u * 4u) && m.load32(copy2 + 4u * 4u) != m.load32(real + 4u * 4u),
              "real blob: the texture CALL is copied as is; only the vertex VADR changes");
        check(read_vertex(m, c2, 4, v) && v[0] == 400, "real blob: the vertices are copied");
        m.store32(real + 4u * 4u, cmd(0x01, 0xA00123u));  // a second VADR pointing elsewhere is not a blob this code can copy
        m.store32(real + 5u * 4u, cmd(0x01, 0xA00456u));
        check(!parse_blob(m, real, r), "real blob: two different VADRs are refused");
    }
    return failures == 0 ? 0 : 1;
}
