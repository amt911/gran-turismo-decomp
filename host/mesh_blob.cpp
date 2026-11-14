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
    if (blob == 0u || !m.contains(blob, 4u)) return false;
    BlobInfo info;
    std::uint32_t base = 0u, vtype = 0u, total = 0u;
    bool have_vadr = false, have_vtype = false;
    for (std::uint32_t i = 0; i < kMaxCommandWords; ++i) {
        const std::uint32_t at = blob + 4u * i;
        if (!m.contains(at, 4u)) return false;
        const std::uint32_t word = m.load32(at);
        switch (op(word)) {
        case kBase: base = param(word); break;
        case kVadr: {
            const std::uint32_t address = ((base << 8) & 0xFF000000u) | param(word);
            if (have_vadr && address != info.vertex_data) return false;
            have_vadr = true;
            info.vertex_data = address;
            break;
        }
        case kVtype: vtype = param(word); have_vtype = true; break;
        case kPrim: total += param(word) & 0xFFFFu; break;
        case kRet: {
            if (!have_vadr || !have_vtype || total == 0u || !format(vtype, info.stride, info.position_offset)) return false;
            info.vertex_count = total;
            info.command_bytes = 4u * (i + 1u);
            if (!m.contains(info.vertex_data, total * info.stride)) return false;
            out = info;
            return true;
        }
        default: break;
        }
    }
    return false;
}

bool parse_runs(const psprecomp::GuestMemory &m, std::uint32_t blob, std::vector<Run> &runs) {
    runs.clear();
    if (blob == 0u || !m.contains(blob, 4u)) return false;
    std::uint32_t base = 0u, vtype = 0u;
    Run cur;
    bool open = false;
    const auto close = [&]() {
        if (open && cur.count > 0u && cur.stride != 0u && runs.size() < kMaxRuns && m.contains(cur.vertex_data, cur.count * cur.stride)) runs.push_back(cur);
        open = false;
        cur = Run{};
    };
    const auto start = [&](std::uint32_t address) {
        close();
        cur.vertex_data = address;
        std::uint32_t stride = 0u, offset = 0u;
        if (format(vtype, stride, offset)) {
            cur.stride = stride;
            cur.position_offset = offset;
        }
        open = true;
    };
    for (std::uint32_t i = 0; i < kMaxCommandWords; ++i) {
        const std::uint32_t at = blob + 4u * i;
        if (!m.contains(at, 4u)) return false;
        const std::uint32_t word = m.load32(at);
        switch (op(word)) {
        case kBase: base = param(word); break;
        case kVadr: start(((base << 8) & 0xFF000000u) | param(word)); break;
        case kVtype: {
            const std::uint32_t before_end = cur.vertex_data + cur.count * (cur.stride != 0u ? cur.stride : 0u);
            const bool continues = open && cur.count > 0u && cur.stride != 0u && param(word) != vtype;
            vtype = param(word);
            if (continues) start(before_end);          // the vertices after a format change follow the run before it
            else if (open && cur.count == 0u) start(cur.vertex_data);  // format of a run that has no PRIM yet
            break;
        }
        case kPrim:
            if (open) cur.count += param(word) & 0xFFFFu;
            break;
        case kRet: close(); return true;
        default: break;
        }
    }
    return false;
}

bool read_run_vertex(const psprecomp::GuestMemory &m, const Run &run, std::uint32_t index, std::int16_t xyz[3]) {
    if (index >= run.count) return false;
    const std::uint32_t at = run.vertex_data + index * run.stride + run.position_offset;
    for (int c = 0; c < 3; ++c) xyz[c] = static_cast<std::int16_t>(m.load16(at + 2u * c));
    return true;
}

bool write_run_vertex(psprecomp::GuestMemory &m, const Run &run, std::uint32_t index, const std::int16_t xyz[3]) {
    if (index >= run.count) return false;
    const std::uint32_t at = run.vertex_data + index * run.stride + run.position_offset;
    for (int c = 0; c < 3; ++c) m.store16(at + 2u * c, static_cast<std::uint16_t>(xyz[c]));
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
    if (!parse_blob(m, blob, info) || !m.contains(destination, copy_bytes(info))) return false;
    for (std::uint32_t i = 0; i < info.command_bytes; ++i) m.store8(destination + i, m.load8(blob + i));
    const std::uint32_t vertices = destination + info.command_bytes;
    for (std::uint32_t i = 0; i < info.vertex_count * info.stride; ++i) m.store8(vertices + i, m.load8(info.vertex_data + i));
    // The VADR word, and the BASE before it (the last BASE in front of it), now name the copy's vertices.
    std::uint32_t base_at = 0u;
    for (std::uint32_t at = destination; at < destination + info.command_bytes; at += 4u) {
        const std::uint32_t word = m.load32(at);
        if (op(word) == kBase) base_at = at;
        else if (op(word) == kVadr) {
            if (base_at != 0u) m.store32(base_at, (kBase << 24) | ((vertices >> 8) & 0xFF0000u));
            m.store32(at, (kVadr << 24) | (vertices & 0xFFFFFFu));
        }
    }
    return true;
}

} // namespace gt::mesh
