// Decodes a PSMF movie from the packets the game puts in its sceMpeg ring buffer (GT's movies are
// scrambled on disc and only readable once the game has descrambled them in guest memory).
#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <vector>

namespace gt {

class MovieDecoder {
public:
    MovieDecoder();
    ~MovieDecoder();
    MovieDecoder(const MovieDecoder &) = delete;
    MovieDecoder &operator=(const MovieDecoder &) = delete;

    // Queues one 2048-byte stream packet, in stream order.
    void push_packet(std::span<const std::uint8_t> packet);

    // Decodes the next picture as RGBA (width * height * 4 bytes), consuming queued packets as
    // needed. Returns false when the queued packets don't complete a picture. `consumed` counts the
    // packets taken from the queue (they are free in the guest ring buffer again).
    bool next_frame(std::vector<std::uint8_t> &rgba, std::uint32_t &width, std::uint32_t &height,
                    std::uint32_t &consumed);

    std::size_t queued_packets() const noexcept { return packets_.size(); }

private:
    struct Codec;
    std::deque<std::vector<std::uint8_t>> packets_;
    std::unique_ptr<Codec> codec_;
};

} // namespace gt
