// Unit test for the ring-buffer movie decoder (host/movie_decoder.cpp). The H.264 stream is synthetic: encoded
// here with libx264 through libavcodec, wrapped in 2048-byte MPEG program-stream packets as GT's ring buffer
// holds them — no game data. Small pictures put several frames in one packet, which is the case the decoder
// must not drop frames in. Skipped (exit 0) when libavcodec has no H.264 encoder.
#include "movie_decoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr int kFrames = 60;
constexpr int kSize = 64;

std::vector<std::uint8_t> encode_stream() {
    std::vector<std::uint8_t> es;
    const AVCodec *codec = avcodec_find_encoder_by_name("libx264");
    if (codec == nullptr) return es;
    AVCodecContext *context = avcodec_alloc_context3(codec);
    context->width = kSize;
    context->height = kSize;
    context->pix_fmt = AV_PIX_FMT_YUV420P;
    context->time_base = {1, 30};
    context->gop_size = 30;
    context->max_b_frames = 0;
    if (avcodec_open2(context, codec, nullptr) < 0) {
        avcodec_free_context(&context);
        return es;
    }
    AVFrame *frame = av_frame_alloc();
    frame->format = context->pix_fmt;
    frame->width = kSize;
    frame->height = kSize;
    av_frame_get_buffer(frame, 0);
    AVPacket *packet = av_packet_alloc();
    const auto drain = [&] {
        while (avcodec_receive_packet(context, packet) == 0) {
            es.insert(es.end(), packet->data, packet->data + packet->size);
            av_packet_unref(packet);
        }
    };
    for (int i = 0; i < kFrames; ++i) {
        av_frame_make_writable(frame);
        for (int y = 0; y < kSize; ++y)
            for (int x = 0; x < kSize; ++x) frame->data[0][y * frame->linesize[0] + x] = static_cast<std::uint8_t>(x + y + i * 3);
        for (int p = 1; p < 3; ++p)
            for (int y = 0; y < kSize / 2; ++y)
                for (int x = 0; x < kSize / 2; ++x) frame->data[p][y * frame->linesize[p] + x] = 128;
        frame->pts = i;
        avcodec_send_frame(context, frame);
        drain();
    }
    avcodec_send_frame(context, nullptr);
    drain();
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&context);
    return es;
}

// 2048-byte packets: an MPEG-2 pack header, one PES packet of stream 0xE0 with as much of `es` as fits, and
// a padding stream for the rest of the last one.
std::vector<std::vector<std::uint8_t>> packetize(const std::vector<std::uint8_t> &es) {
    std::vector<std::vector<std::uint8_t>> packets;
    constexpr std::size_t kPack = 14u, kPes = 9u, kPayload = 2048u - kPack - kPes;
    for (std::size_t at = 0; at < es.size(); at += kPayload) {
        const std::size_t n = std::min(kPayload, es.size() - at);
        std::vector<std::uint8_t> p = {0x00, 0x00, 0x01, 0xBA, 0x44, 0x00, 0x04, 0x00, 0x04, 0x01, 0x01, 0x89, 0xC3, 0xF8};
        const std::size_t pes_length = 3u + n;
        p.insert(p.end(), {0x00, 0x00, 0x01, 0xE0, static_cast<std::uint8_t>(pes_length >> 8u),
                           static_cast<std::uint8_t>(pes_length & 0xFFu), 0x81, 0x00, 0x00});
        p.insert(p.end(), es.begin() + static_cast<std::ptrdiff_t>(at), es.begin() + static_cast<std::ptrdiff_t>(at + n));
        if (p.size() + 6u <= 2048u) {
            const std::size_t pad = 2048u - p.size() - 6u;
            p.insert(p.end(), {0x00, 0x00, 0x01, 0xBE, static_cast<std::uint8_t>(pad >> 8u), static_cast<std::uint8_t>(pad & 0xFFu)});
        }
        p.resize(2048u, 0xFFu);
        packets.push_back(std::move(p));
    }
    return packets;
}

} // namespace

int main() {
    av_log_set_level(AV_LOG_QUIET);
    const std::vector<std::uint8_t> es = encode_stream();
    if (es.empty()) {
        std::puts("SKIP: no libx264 encoder in libavcodec");
        return 0;
    }
    const auto packets = packetize(es);
    gt::MovieDecoder decoder;
    for (const auto &p : packets) decoder.push_packet(p);
    int decoded = 0;
    std::uint32_t consumed_total = 0u;
    std::vector<std::uint8_t> rgba;
    std::uint32_t width = 0u, height = 0u, consumed = 0u;
    while (decoder.next_frame(rgba, width, height, consumed)) {
        consumed_total += consumed;
        ++decoded;
        if (width != kSize || height != kSize || rgba.size() != kSize * kSize * 4u) {
            std::fprintf(stderr, "FAIL: picture %d is %ux%u\n", decoded, width, height);
            return 1;
        }
    }
    consumed_total += consumed;
    // The parser keeps the last picture until a later start code arrives, so it may stay undecoded.
    if (decoded < kFrames - 1) {
        std::fprintf(stderr, "FAIL: %d of %d pictures decoded from %zu packets (%zu bytes)\n", decoded, kFrames,
                     packets.size(), es.size());
        return 1;
    }
    if (consumed_total != packets.size()) {
        std::fprintf(stderr, "FAIL: %u of %zu packets consumed\n", consumed_total, packets.size());
        return 1;
    }
    std::printf("ok: %d of %d pictures from %zu packets\n", decoded, kFrames, packets.size());
    return 0;
}
