// PSMF (PSP movie container) header parsing for the scePsmf HLE (host/psmf_hle.cpp). Pure: no
// runtime, unit-tested in host/tests/psmf_test.cpp. Layout as documented by PPSSPP's scePsmf.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace gt::psmf {

// scePsmf stream types; kAnyAudio matches ATRAC and PCM in queries.
constexpr std::int32_t kAvc = 0;
constexpr std::int32_t kAtrac = 1;
constexpr std::int32_t kPcm = 2;
constexpr std::int32_t kAnyAudio = 15;

constexpr std::size_t kHeaderSize = 2048u;

struct Stream {
    std::uint8_t stream_id{};       // 0xE0-0xEF video, 0xBD private (audio)
    std::uint8_t private_id{};
    std::int32_t type{-1};
    std::uint32_t channel{};
    std::uint32_t ep_map_offset{};  // from the start of the header
    std::uint32_t ep_entries{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t audio_channels{};
    std::uint32_t audio_frequency{};
};

struct EpEntry {
    std::uint32_t index{};
    std::uint32_t pic_offset{};
    std::uint32_t pts{};
    std::uint32_t offset{};
};

struct Header {
    std::uint32_t raw_version{};    // bytes 4..7 read little-endian ('0015' -> 0x35313030)
    std::uint32_t stream_offset{};
    std::uint32_t stream_size{};
    std::uint32_t start_time{};
    std::uint32_t end_time{};
    std::vector<Stream> streams;
};

// nullopt unless the buffer holds a "PSMF" header of a known version (0012-0015) whose stream table
// fits in it.
std::optional<Header> parse(std::span<const std::uint8_t> bytes);

int count_streams(const Header &header, std::int32_t type);

// The EP (entry point) map of `stream`, read from the same header bytes; empty if it doesn't fit.
std::vector<EpEntry> ep_map(std::span<const std::uint8_t> bytes, const Stream &stream);

// The last entry whose pts is <= `pts`; nullopt if `pts` precedes every entry.
std::optional<EpEntry> find_ep(const std::vector<EpEntry> &map, std::uint32_t pts);

} // namespace gt::psmf
