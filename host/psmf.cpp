#include "psmf.hpp"

namespace gt::psmf {
namespace {

constexpr std::size_t kNumStreamsOffset = 0x80u;
constexpr std::size_t kStreamTableOffset = 0x82u;
constexpr std::size_t kStreamEntrySize = 16u;
constexpr std::size_t kEpEntrySize = 10u;

std::uint32_t be32(std::span<const std::uint8_t> b, std::size_t at) {
    return (static_cast<std::uint32_t>(b[at]) << 24u) | (static_cast<std::uint32_t>(b[at + 1u]) << 16u) |
           (static_cast<std::uint32_t>(b[at + 2u]) << 8u) | static_cast<std::uint32_t>(b[at + 3u]);
}

// 6-byte timestamp, combined the way PPSSPP's getMpegTimeStamp does (the top byte overlaps).
std::uint64_t timestamp(std::span<const std::uint8_t> b, std::size_t at) {
    return static_cast<std::uint64_t>(b[at + 5u]) | (static_cast<std::uint64_t>(b[at + 4u]) << 8u) |
           (static_cast<std::uint64_t>(b[at + 3u]) << 16u) | (static_cast<std::uint64_t>(b[at + 2u]) << 24u) |
           (static_cast<std::uint64_t>(b[at + 1u]) << 32u) | (static_cast<std::uint64_t>(b[at]) << 36u);
}

} // namespace

std::optional<Header> parse(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < kStreamTableOffset) return std::nullopt;
    if (bytes[0] != 'P' || bytes[1] != 'S' || bytes[2] != 'M' || bytes[3] != 'F') return std::nullopt;
    Header header;
    header.raw_version = static_cast<std::uint32_t>(bytes[4]) | (static_cast<std::uint32_t>(bytes[5]) << 8u) |
                         (static_cast<std::uint32_t>(bytes[6]) << 16u) | (static_cast<std::uint32_t>(bytes[7]) << 24u);
    switch (header.raw_version) {
    case 0x32313030u: case 0x33313030u: case 0x34313030u: case 0x35313030u: break;  // "0012".."0015"
    default: return std::nullopt;
    }
    header.stream_offset = be32(bytes, 0x08u);
    header.stream_size = be32(bytes, 0x0Cu);
    header.start_time = static_cast<std::uint32_t>(timestamp(bytes, 0x54u));
    header.end_time = static_cast<std::uint32_t>(timestamp(bytes, 0x5Au));
    const std::size_t count = (static_cast<std::size_t>(bytes[kNumStreamsOffset]) << 8u) | bytes[kNumStreamsOffset + 1u];
    if (kStreamTableOffset + count * kStreamEntrySize > bytes.size()) return std::nullopt;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = kStreamTableOffset + i * kStreamEntrySize;
        Stream stream;
        stream.stream_id = bytes[at];
        stream.private_id = bytes[at + 1u];
        if ((stream.stream_id & 0xF0u) == 0xE0u) {
            stream.type = kAvc;
            stream.channel = stream.stream_id & 0x0Fu;
            stream.ep_map_offset = be32(bytes, at + 4u);
            stream.ep_entries = be32(bytes, at + 8u);
            stream.width = static_cast<std::uint32_t>(bytes[at + 12u]) * 16u;
            stream.height = static_cast<std::uint32_t>(bytes[at + 13u]) * 16u;
        } else if (stream.stream_id == 0xBDu) {
            stream.type = (stream.private_id & 0xF0u) != 0u ? kPcm : kAtrac;
            stream.channel = stream.private_id & 0x0Fu;
            stream.audio_channels = bytes[at + 14u];
            stream.audio_frequency = bytes[at + 15u];
        }
        header.streams.push_back(stream);
    }
    return header;
}

int count_streams(const Header &header, std::int32_t type) {
    int count = 0;
    for (const Stream &stream : header.streams) {
        const bool audio = stream.type == kAtrac || stream.type == kPcm;
        if (stream.type == type || (type == kAnyAudio && audio)) ++count;
    }
    return count;
}

std::vector<EpEntry> ep_map(std::span<const std::uint8_t> bytes, const Stream &stream) {
    std::vector<EpEntry> map;
    if (stream.type != kAvc) return map;
    const std::uint64_t end = stream.ep_map_offset + static_cast<std::uint64_t>(stream.ep_entries) * kEpEntrySize;
    if (end > bytes.size()) return map;
    for (std::uint32_t i = 0; i < stream.ep_entries; ++i) {
        const std::size_t at = stream.ep_map_offset + i * kEpEntrySize;
        map.push_back(EpEntry{bytes[at], bytes[at + 1u], be32(bytes, at + 2u), be32(bytes, at + 6u)});
    }
    return map;
}

std::optional<EpEntry> find_ep(const std::vector<EpEntry> &map, std::uint32_t pts) {
    std::optional<EpEntry> found;
    for (const EpEntry &entry : map) {
        if (entry.pts > pts) break;
        found = entry;
    }
    return found;
}

} // namespace gt::psmf
