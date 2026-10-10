#include "atrac_decoder.hpp"

#include <algorithm>
#include <cmath>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
}

namespace gt {

struct AtracFrameDecoder::State {
    AVCodecContext *codec{};
    AVPacket *packet{};
    AVFrame *frame{};
    ~State() {
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec);
    }
};

AtracFrameDecoder::AtracFrameDecoder() = default;
AtracFrameDecoder::~AtracFrameDecoder() = default;
AtracFrameDecoder::AtracFrameDecoder(AtracFrameDecoder &&) noexcept = default;
AtracFrameDecoder &AtracFrameDecoder::operator=(AtracFrameDecoder &&) noexcept = default;

bool AtracFrameDecoder::open(std::uint32_t channels, std::uint32_t sample_rate, std::uint32_t block_align) {
    state_.reset();
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_ATRAC3P);
    if (codec == nullptr || channels == 0u || channels > 2u || block_align == 0u) return false;
    auto state = std::make_unique<State>();
    state->codec = avcodec_alloc_context3(codec);
    state->packet = av_packet_alloc();
    state->frame = av_frame_alloc();
    if (state->codec == nullptr || state->packet == nullptr || state->frame == nullptr) return false;
    av_channel_layout_default(&state->codec->ch_layout, static_cast<int>(channels));
    state->codec->sample_rate = static_cast<int>(sample_rate);
    state->codec->block_align = static_cast<int>(block_align);
    if (avcodec_open2(state->codec, codec, nullptr) < 0) return false;
    state_ = std::move(state);
    return true;
}

bool AtracFrameDecoder::is_open() const noexcept { return state_ != nullptr; }

void AtracFrameDecoder::reset() noexcept {
    if (state_) avcodec_flush_buffers(state_->codec);
}

std::vector<std::int16_t> AtracFrameDecoder::decode(std::span<const std::uint8_t> frame) {
    std::vector<std::int16_t> pcm;
    if (!state_) return pcm;
    State &s = *state_;
    if (av_new_packet(s.packet, static_cast<int>(frame.size())) < 0) return pcm;
    std::copy(frame.begin(), frame.end(), s.packet->data);
    const int sent = avcodec_send_packet(s.codec, s.packet);
    av_packet_unref(s.packet);
    if (sent < 0) return pcm;
    while (avcodec_receive_frame(s.codec, s.frame) == 0) {
        const int channels = s.frame->ch_layout.nb_channels;
        const int samples = s.frame->nb_samples;
        const auto sample_at = [&](int channel, int index) -> float {
            const int c = std::min(channel, channels - 1);
            switch (s.frame->format) {
            case AV_SAMPLE_FMT_FLTP: return reinterpret_cast<const float *>(s.frame->extended_data[c])[index];
            case AV_SAMPLE_FMT_FLT: return reinterpret_cast<const float *>(s.frame->data[0])[index * channels + c];
            case AV_SAMPLE_FMT_S16P: return reinterpret_cast<const std::int16_t *>(s.frame->extended_data[c])[index] / 32768.0f;
            case AV_SAMPLE_FMT_S16: return reinterpret_cast<const std::int16_t *>(s.frame->data[0])[index * channels + c] / 32768.0f;
            default: return 0.0f;
            }
        };
        for (int index = 0; index < samples; ++index) {
            for (int channel = 0; channel < 2; ++channel) {
                const float value = std::clamp(sample_at(channel, index) * 32768.0f, -32768.0f, 32767.0f);
                pcm.push_back(static_cast<std::int16_t>(std::lrint(value)));
            }
        }
        av_frame_unref(s.frame);
    }
    return pcm;
}

} // namespace gt
