// MPEG-2 program-stream demuxing for PSMF movie packets (host/ps_demux.cpp). Pure: unit-tested in
// host/tests/ps_demux_test.cpp.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace gt {

// Appends to `video` the payload of every PES packet of `stream_id` (e.g. 0xE0, the first video
// stream) found in one 2048-byte PSMF packet. Pack and system headers, other streams and padding are
// skipped; a PES cut short by the end of the packet contributes the bytes that are there.
void demux_video_packet(std::span<const std::uint8_t> packet, std::uint8_t stream_id,
                        std::vector<std::uint8_t> &video);

// Appends to `audio` the ATRAC3plus elementary stream carried in one PSMF packet: the payload of
// every private stream 1 (0xBD) PES packet, without the 4-byte substream header PSMF puts first.
void demux_audio_packet(std::span<const std::uint8_t> packet, std::vector<std::uint8_t> &audio);

// Splits a PSMF ATRAC3plus elementary stream into frames. Each frame starts with an 8-byte header
// whose first two bytes are the sync 0F D0; every frame of a stream has the same size, measured as
// the distance between the first two syncs. Bytes that are not at a sync are skipped up to the next.
class Atrac3pFramer {
public:
    void push(std::span<const std::uint8_t> bytes);
    // The payload (frame without its header) of the next complete frame; false when none is queued.
    bool next(std::vector<std::uint8_t> &payload);
    std::size_t frame_size() const noexcept { return frame_size_; }  // 0 until known
    void reset() noexcept;

    static constexpr std::size_t kHeaderSize = 8u;

private:
    std::vector<std::uint8_t> bytes_;
    std::size_t cursor_{};
    std::size_t frame_size_{};
};

} // namespace gt
