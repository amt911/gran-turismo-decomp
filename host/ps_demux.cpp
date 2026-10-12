#include "ps_demux.hpp"

namespace gt {
namespace {

constexpr std::uint8_t kPackHeader = 0xBAu;
constexpr std::uint8_t kProgramEnd = 0xB9u;

bool start_code_at(std::span<const std::uint8_t> b, std::size_t at) {
    return at + 4u <= b.size() && b[at] == 0u && b[at + 1u] == 0u && b[at + 2u] == 1u;
}

// Appends the payload of every PES packet of `stream_id` in `packet`, dropping its first `skip` bytes.
void demux_stream(std::span<const std::uint8_t> packet, std::uint8_t stream_id, std::size_t skip,
                  std::vector<std::uint8_t> &out) {
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
            const std::size_t payload = body + 3u + packet[body + 2u] + skip;  // MPEG-2 PES header
            if (payload < end) out.insert(out.end(), packet.begin() + payload, packet.begin() + end);
        }
        at = body + length;
    }
}

constexpr std::uint8_t kPrivateStream1 = 0xBDu;
constexpr std::size_t kPsmfSubstreamHeader = 4u;

} // namespace

void demux_video_packet(std::span<const std::uint8_t> packet, std::uint8_t stream_id,
                        std::vector<std::uint8_t> &video) {
    demux_stream(packet, stream_id, 0u, video);
}

void demux_audio_packet(std::span<const std::uint8_t> packet, std::vector<std::uint8_t> &audio) {
    demux_stream(packet, kPrivateStream1, kPsmfSubstreamHeader, audio);
}

namespace {

bool sync_at(const std::vector<std::uint8_t> &b, std::size_t at) {
    return at + 1u < b.size() && b[at] == 0x0Fu && b[at + 1u] == 0xD0u;
}

} // namespace

void Atrac3pFramer::push(std::span<const std::uint8_t> bytes) {
    if (cursor_ > 0u && cursor_ * 2u >= bytes_.size()) {  // drop what was consumed, now and then
        bytes_.erase(bytes_.begin(), bytes_.begin() + static_cast<std::ptrdiff_t>(cursor_));
        cursor_ = 0u;
    }
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    if (frame_size_ != 0u) return;
    std::size_t first = cursor_;
    while (first < bytes_.size() && !sync_at(bytes_, first)) ++first;
    for (std::size_t at = first + kHeaderSize; at + 1u < bytes_.size(); ++at) {
        if (sync_at(bytes_, at)) {
            frame_size_ = at - first;
            return;
        }
    }
}

bool Atrac3pFramer::next(std::vector<std::uint8_t> &payload) {
    while (cursor_ < bytes_.size() && !sync_at(bytes_, cursor_)) ++cursor_;
    if (!sync_at(bytes_, cursor_)) return false;
    if (frame_size_ == 0u) return false;
    if (cursor_ + frame_size_ > bytes_.size()) return false;
    payload.assign(bytes_.begin() + static_cast<std::ptrdiff_t>(cursor_ + kHeaderSize),
                   bytes_.begin() + static_cast<std::ptrdiff_t>(cursor_ + frame_size_));
    cursor_ += frame_size_;
    return true;
}

void Atrac3pFramer::reset() noexcept {
    bytes_.clear();
    cursor_ = 0u;
    frame_size_ = 0u;
}

} // namespace gt
