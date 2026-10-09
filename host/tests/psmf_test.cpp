// Unit tests for the PSMF header parser (host/psmf.cpp). The headers are synthetic, built here
// field by field — no game data. Plain executable, run by CTest.
#include "psmf.hpp"

#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

void put_be32(std::vector<std::uint8_t> &b, std::size_t at, std::uint32_t v) {
    b[at] = static_cast<std::uint8_t>(v >> 24u);
    b[at + 1u] = static_cast<std::uint8_t>(v >> 16u);
    b[at + 2u] = static_cast<std::uint8_t>(v >> 8u);
    b[at + 3u] = static_cast<std::uint8_t>(v);
}

// One AVC stream (480x272, 3 EP entries at 0x100) and one ATRAC stream.
std::vector<std::uint8_t> sample_header() {
    std::vector<std::uint8_t> b(2048u, 0u);
    b[0] = 'P'; b[1] = 'S'; b[2] = 'M'; b[3] = 'F';
    b[4] = '0'; b[5] = '0'; b[6] = '1'; b[7] = '5';
    put_be32(b, 0x08u, 0x800u);      // stream data offset
    put_be32(b, 0x0Cu, 0x123000u);   // stream data size
    b[0x54u + 5u] = 0x90u;           // first timestamp: 90000 = 0x015F90
    b[0x54u + 4u] = 0x5Fu;
    b[0x54u + 3u] = 0x01u;
    b[0x5Au + 5u] = 0x90u;           // last timestamp: 0x0A0000 + 0x90
    b[0x5Au + 3u] = 0x0Au;
    b[0x80u] = 0u; b[0x81u] = 2u;    // two streams
    const std::size_t video = 0x82u;
    b[video] = 0xE0u;
    put_be32(b, video + 4u, 0x100u); // EP map offset
    put_be32(b, video + 8u, 3u);     // EP entries
    b[video + 12u] = 480u / 16u;
    b[video + 13u] = 272u / 16u;
    const std::size_t audio = 0x92u;
    b[audio] = 0xBDu;
    b[audio + 1u] = 0x00u;           // ATRAC, channel 0
    b[audio + 14u] = 2u;
    b[audio + 15u] = 2u;
    for (std::uint32_t i = 0; i < 3u; ++i) {  // EP map: index, pic offset, pts, offset
        const std::size_t at = 0x100u + 10u * i;
        b[at] = static_cast<std::uint8_t>(i);
        b[at + 1u] = static_cast<std::uint8_t>(0x10u + i);
        put_be32(b, at + 2u, 90000u + 45000u * i);
        put_be32(b, at + 6u, 0x800u * i);
    }
    return b;
}

} // namespace

int main() {
    const auto bytes = sample_header();
    {
        const auto header = gt::psmf::parse(bytes);
        check(header.has_value(), "valid header parses");
        if (header) {
            check(header->raw_version == 0x35313030u, "version is the raw little-endian '0015'");
            check(header->stream_offset == 0x800u && header->stream_size == 0x123000u, "stream offset/size");
            check(header->start_time == 90000u, "presentation start time");
            check(header->end_time == 0x0A0090u, "presentation end time");
            check(header->streams.size() == 2u, "two streams");
            check(gt::psmf::count_streams(*header, gt::psmf::kAvc) == 1, "one AVC stream");
            check(gt::psmf::count_streams(*header, gt::psmf::kAtrac) == 1, "one ATRAC stream");
            check(gt::psmf::count_streams(*header, gt::psmf::kPcm) == 0, "no PCM stream");
            check(gt::psmf::count_streams(*header, gt::psmf::kAnyAudio) == 1, "type 15 counts any audio");
            const auto &video = header->streams[0];
            check(video.type == gt::psmf::kAvc && video.width == 480u && video.height == 272u, "video info");
            check(header->streams[1].type == gt::psmf::kAtrac && header->streams[1].channel == 0u,
                  "audio stream type/channel");
            const auto map = gt::psmf::ep_map(bytes, video);
            check(map.size() == 3u, "three EP entries");
            if (map.size() == 3u)
                check(map[2].pts == 180000u && map[2].offset == 0x1000u && map[2].pic_offset == 0x12u,
                      "EP entry fields");
            const auto at_start = gt::psmf::find_ep(map, 90000u);
            check(at_start && at_start->index == 0u, "EP at the first pts");
            const auto between = gt::psmf::find_ep(map, 150000u);
            check(between && between->index == 1u, "EP is the last entry at or before pts");
            check(!gt::psmf::find_ep(map, 1000u), "no EP before the first entry");
        }
    }
    {
        auto bad = bytes;
        bad[0] = 'X';
        check(!gt::psmf::parse(bad).has_value(), "wrong magic is rejected");
        auto unknown = bytes;
        unknown[7] = '9';
        check(!gt::psmf::parse(unknown).has_value(), "unknown version is rejected");
        check(!gt::psmf::parse(std::span<const std::uint8_t>(bytes.data(), 0x40u)).has_value(),
              "short buffer is rejected");
    }
    if (failures == 0) std::puts("psmf: all tests passed");
    return failures == 0 ? 0 : 1;
}
