// ATRAC3plus frames to 16-bit stereo PCM with FFmpeg (host/atrac_decoder.cpp), for sceAtrac contexts
// whose data the game streams from memory (GT keeps its music inside GT.VOL, so there is no file).
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace gt {

class AtracFrameDecoder {
public:
    AtracFrameDecoder();
    ~AtracFrameDecoder();
    AtracFrameDecoder(const AtracFrameDecoder &) = delete;
    AtracFrameDecoder &operator=(const AtracFrameDecoder &) = delete;
    AtracFrameDecoder(AtracFrameDecoder &&) noexcept;
    AtracFrameDecoder &operator=(AtracFrameDecoder &&) noexcept;

    // ATRAC3plus with this stream's channels, rate and frame size (block_align).
    bool open(std::uint32_t channels, std::uint32_t sample_rate, std::uint32_t block_align);
    bool is_open() const noexcept;
    void reset() noexcept;  // after a seek: the next frame does not follow the last one
    // Decodes one frame into interleaved stereo samples (mono is duplicated); empty on error.
    std::vector<std::int16_t> decode(std::span<const std::uint8_t> frame);

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace gt
