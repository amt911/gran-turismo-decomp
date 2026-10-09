// Unit tests for the MPEG program-stream demuxer (host/ps_demux.cpp). Packets are synthetic, built
// here — no game data. Plain executable, run by CTest.
#include "ps_demux.hpp"

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

void append(std::vector<std::uint8_t> &out, std::initializer_list<std::uint8_t> bytes) {
    out.insert(out.end(), bytes);
}

// MPEG-2 pack header with `stuffing` stuffing bytes.
void pack_header(std::vector<std::uint8_t> &out, std::uint8_t stuffing) {
    append(out, {0x00, 0x00, 0x01, 0xBA, 0x44, 0x00, 0x04, 0x00, 0x04, 0x01, 0x01, 0x89, 0xC3});
    out.push_back(static_cast<std::uint8_t>(0xF8u | stuffing));
    for (std::uint8_t i = 0; i < stuffing; ++i) out.push_back(0xFFu);
}

// PES packet: stream id, `header_data` optional-header bytes, then the payload.
void pes(std::vector<std::uint8_t> &out, std::uint8_t id, std::vector<std::uint8_t> header_data,
         std::vector<std::uint8_t> payload) {
    const std::size_t length = 3u + header_data.size() + payload.size();
    append(out, {0x00, 0x00, 0x01, id, static_cast<std::uint8_t>(length >> 8u), static_cast<std::uint8_t>(length)});
    append(out, {0x81, 0x80, static_cast<std::uint8_t>(header_data.size())});
    out.insert(out.end(), header_data.begin(), header_data.end());
    out.insert(out.end(), payload.begin(), payload.end());
}

} // namespace

int main() {
    {
        std::vector<std::uint8_t> packet;
        pack_header(packet, 2u);
        pes(packet, 0xE0u, {0x21, 0x00, 0x01, 0x00, 0x01}, {0x00, 0x00, 0x00, 0x01, 0x67, 0xAA});
        pes(packet, 0xBDu, {}, {0x00, 0x11, 0x22});  // audio: ignored
        pes(packet, 0xE0u, {}, {0xBB, 0xCC});
        packet.resize(2048u, 0xFFu);                 // padding to the packet size
        std::vector<std::uint8_t> video;
        gt::demux_video_packet(packet, 0xE0u, video);
        const std::vector<std::uint8_t> expected{0x00, 0x00, 0x00, 0x01, 0x67, 0xAA, 0xBB, 0xCC};
        check(video == expected, "video payloads of stream E0 are concatenated, audio skipped");
    }
    {
        std::vector<std::uint8_t> packet;
        pack_header(packet, 0u);
        append(packet, {0x00, 0x00, 0x01, 0xBB, 0x00, 0x02, 0x12, 0x34});  // system header
        pes(packet, 0xE1u, {}, {0x01});                                    // other video stream
        pes(packet, 0xE0u, {}, {0x02});
        append(packet, {0x00, 0x00, 0x01, 0xBE, 0x00, 0x03, 0xFF, 0xFF, 0xFF});  // padding stream
        pes(packet, 0xE0u, {}, {0x03});
        std::vector<std::uint8_t> video{0x99};
        gt::demux_video_packet(packet, 0xE0u, video);
        const std::vector<std::uint8_t> expected{0x99, 0x02, 0x03};
        check(video == expected, "system header, other streams and padding are skipped; output appends");
    }
    {
        std::vector<std::uint8_t> packet;
        pack_header(packet, 0u);
        pes(packet, 0xE0u, {}, {0x01, 0x02, 0x03, 0x04});
        packet.resize(packet.size() - 2u);  // truncated PES: take what is there
        std::vector<std::uint8_t> video;
        gt::demux_video_packet(packet, 0xE0u, video);
        const std::vector<std::uint8_t> expected{0x01, 0x02};
        check(video == expected, "a PES cut by the end of the packet yields its available bytes");
    }
    {
        std::vector<std::uint8_t> garbage(2048u, 0x00u);
        std::vector<std::uint8_t> video;
        gt::demux_video_packet(garbage, 0xE0u, video);
        check(video.empty(), "a packet without start codes yields nothing");
    }
    if (failures == 0) std::puts("ps_demux: all tests passed");
    return failures == 0 ? 0 : 1;
}
