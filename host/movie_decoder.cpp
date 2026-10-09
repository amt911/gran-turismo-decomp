#include "movie_decoder.hpp"

#include "ps_demux.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace gt {

struct MovieDecoder::Codec {
    AVCodecContext *context{};
    AVCodecParserContext *parser{};
    AVPacket *packet{};
    AVFrame *frame{};
    SwsContext *scaler{};
    int scaler_width{};
    int scaler_height{};
    int scaler_format{-1};

    Codec() {
        const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (codec == nullptr) return;
        context = avcodec_alloc_context3(codec);
        parser = av_parser_init(AV_CODEC_ID_H264);
        packet = av_packet_alloc();
        frame = av_frame_alloc();
        if (context != nullptr && avcodec_open2(context, codec, nullptr) < 0) avcodec_free_context(&context);
    }
    ~Codec() {
        if (scaler != nullptr) sws_freeContext(scaler);
        av_frame_free(&frame);
        av_packet_free(&packet);
        if (parser != nullptr) av_parser_close(parser);
        avcodec_free_context(&context);
    }
    bool ready() const { return context != nullptr && parser != nullptr && packet != nullptr && frame != nullptr; }

    // Feeds elementary-stream bytes through the parser into the decoder.
    void feed(const std::vector<std::uint8_t> &es) {
        const std::uint8_t *data = es.data();
        int size = static_cast<int>(es.size());
        while (size > 0) {
            const int used = av_parser_parse2(parser, context, &packet->data, &packet->size, data, size,
                                              AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (used < 0) return;
            data += used;
            size -= used;
            if (packet->size > 0) (void)avcodec_send_packet(context, packet);
        }
    }

    bool receive(std::vector<std::uint8_t> &rgba, std::uint32_t &width, std::uint32_t &height) {
        if (avcodec_receive_frame(context, frame) != 0) return false;
        if (scaler == nullptr || scaler_width != frame->width || scaler_height != frame->height ||
            scaler_format != frame->format) {
            if (scaler != nullptr) sws_freeContext(scaler);
            scaler = sws_getContext(frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
                                    frame->width, frame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr,
                                    nullptr, nullptr);
            scaler_width = frame->width;
            scaler_height = frame->height;
            scaler_format = frame->format;
        }
        if (scaler == nullptr) return false;
        width = static_cast<std::uint32_t>(frame->width);
        height = static_cast<std::uint32_t>(frame->height);
        rgba.resize(static_cast<std::size_t>(width) * height * 4u);
        std::uint8_t *planes[1]{rgba.data()};
        const int strides[1]{static_cast<int>(width * 4u)};
        sws_scale(scaler, frame->data, frame->linesize, 0, frame->height, planes, strides);
        av_frame_unref(frame);
        return true;
    }
};

MovieDecoder::MovieDecoder() : codec_(std::make_unique<Codec>()) {}
MovieDecoder::~MovieDecoder() = default;

void MovieDecoder::push_packet(std::span<const std::uint8_t> packet) {
    packets_.emplace_back(packet.begin(), packet.end());
}

bool MovieDecoder::next_frame(std::vector<std::uint8_t> &rgba, std::uint32_t &width, std::uint32_t &height,
                              std::uint32_t &consumed) {
    consumed = 0u;
    if (!codec_->ready()) return false;
    std::vector<std::uint8_t> es;
    for (;;) {
        if (codec_->receive(rgba, width, height)) return true;
        if (packets_.empty()) return false;
        es.clear();
        demux_video_packet(packets_.front(), 0xE0u, es);
        packets_.pop_front();
        ++consumed;
        codec_->feed(es);
    }
}

} // namespace gt
