#include "mesh_blob.hpp"

namespace gt::mesh {

namespace {
constexpr std::uint32_t kBase = 0x10u, kVadr = 0x01u, kVtype = 0x12u, kPrim = 0x04u, kRet = 0x0Bu;
std::uint32_t op(std::uint32_t word) { return word >> 24; }
std::uint32_t param(std::uint32_t word) { return word & 0xFFFFFFu; }
bool format(std::uint32_t vtype, std::uint32_t &stride, std::uint32_t &position_offset) {
    switch (vtype) {
    case 0x122u: stride = 14u; position_offset = 8u; return true;
    case 0x130u:
    case 0x112u: stride = 12u; position_offset = 6u; return true;
    default: return false;
    }
}
} // namespace

bool parse_blob(const psprecomp::GuestMemory &m, std::uint32_t blob, BlobInfo &out) {
    if (blob == 0u || !m.contains(blob, kBlobHeaderBytes)) return false;
    const std::uint32_t base = m.load32(blob), vadr = m.load32(blob + 4u), vtype = m.load32(blob + 8u), prim = m.load32(blob + 12u),
                        ret = m.load32(blob + 16u);
    if (op(base) != kBase || op(vadr) != kVadr || op(vtype) != kVtype || op(prim) != kPrim || op(ret) != kRet) return false;
    BlobInfo info;
    if (!format(param(vtype), info.stride, info.position_offset)) return false;
    info.vertex_count = param(prim) & 0xFFFFu;
    if (info.vertex_count == 0u) return false;
    info.vertex_data = ((param(base) << 8) & 0xFF000000u) | param(vadr);
    const std::uint32_t bytes = info.vertex_count * info.stride;
    if (!m.contains(info.vertex_data, bytes)) return false;
    info.byte_size = info.vertex_data == blob + kBlobHeaderBytes ? kBlobHeaderBytes + bytes : kBlobHeaderBytes;
    out = info;
    return true;
}

bool read_vertex(const psprecomp::GuestMemory &m, const BlobInfo &info, std::uint32_t index, std::int16_t xyz[3]) {
    if (index >= info.vertex_count) return false;
    const std::uint32_t at = info.vertex_data + index * info.stride + info.position_offset;
    for (int c = 0; c < 3; ++c) xyz[c] = static_cast<std::int16_t>(m.load16(at + 2u * c));
    return true;
}

bool write_vertex(psprecomp::GuestMemory &m, const BlobInfo &info, std::uint32_t index, const std::int16_t xyz[3]) {
    if (index >= info.vertex_count) return false;
    const std::uint32_t at = info.vertex_data + index * info.stride + info.position_offset;
    for (int c = 0; c < 3; ++c) m.store16(at + 2u * c, static_cast<std::uint16_t>(xyz[c]));
    return true;
}

bool copy_blob(psprecomp::GuestMemory &m, std::uint32_t blob, std::uint32_t destination) {
    BlobInfo info;
    if (!parse_blob(m, blob, info) || info.byte_size == kBlobHeaderBytes || !m.contains(destination, info.byte_size)) return false;
    for (std::uint32_t i = 0; i < info.byte_size; ++i) m.store8(destination + i, m.load8(blob + i));
    const std::uint32_t vertices = destination + kBlobHeaderBytes;
    m.store32(destination, (kBase << 24) | ((vertices >> 8) & 0xFF0000u));
    m.store32(destination + 4u, (kVadr << 24) | (vertices & 0xFFFFFFu));
    return true;
}

bool last_world_translation(const psprecomp::GuestMemory &m, std::uint32_t cursor, std::uint32_t max_words, float xyz[3]) {
    constexpr std::uint32_t kWorldN = 0x3Au, kWorldD = 0x3Bu;
    for (std::uint32_t back = 13u; back <= max_words; ++back) {
        const std::uint32_t at = cursor - 4u * back;
        if (!m.contains(at, 13u * 4u) || op(m.load32(at)) != kWorldN) continue;
        bool all = true;
        for (std::uint32_t i = 1; i <= 12u && all; ++i) all = op(m.load32(at + 4u * i)) == kWorldD;
        if (!all) continue;
        for (std::uint32_t c = 0; c < 3u; ++c) {
            const std::uint32_t bits = param(m.load32(at + 4u * (10u + c))) << 8;
            float f;
            static_assert(sizeof f == sizeof bits);
            __builtin_memcpy(&f, &bits, sizeof f);
            xyz[c] = f;
        }
        return true;
    }
    return false;
}

} // namespace gt::mesh
