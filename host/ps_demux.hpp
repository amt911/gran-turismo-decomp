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

} // namespace gt
