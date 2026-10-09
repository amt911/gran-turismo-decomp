#include "ps_demux.hpp"

namespace gt {
namespace {

constexpr std::uint8_t kPackHeader = 0xBAu;
constexpr std::uint8_t kProgramEnd = 0xB9u;

bool start_code_at(std::span<const std::uint8_t> b, std::size_t at) {
    return at + 4u <= b.size() && b[at] == 0u && b[at + 1u] == 0u && b[at + 2u] == 1u;
}

} // namespace

void demux_video_packet(std::span<const std::uint8_t> packet, std::uint8_t stream_id,
                        std::vector<std::uint8_t> &video) {
    std::size_t at = 0;
    while (at + 4u <= packet.size()) {
        if (!start_code_at(packet, at)) {
            ++at;  // resynchronise on the next start code
            continue;
        }
        const std::uint8_t id = packet[at + 3u];
        if (id == kProgramEnd) return;
        if (id == kPackHeader) {
            if (at + 14u > packet.size()) return;
            at += 14u + (packet[at + 13u] & 7u);  // MPEG-2 pack header + stuffing
            continue;
        }
        if (id < 0xB9u) {  // not a system-level start code: skip it
            at += 4u;
            continue;
        }
        if (at + 6u > packet.size()) return;
        const std::size_t length = (static_cast<std::size_t>(packet[at + 4u]) << 8u) | packet[at + 5u];
        const std::size_t body = at + 6u;
        const std::size_t end = body + length < packet.size() ? body + length : packet.size();
        if (id == stream_id && body + 3u <= end) {
            const std::size_t payload = body + 3u + packet[body + 2u];  // MPEG-2 PES header
            if (payload < end) video.insert(video.end(), packet.begin() + payload, packet.begin() + end);
        }
        at = body + length;
    }
}

} // namespace gt
