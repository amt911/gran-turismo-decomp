#include "audio_dump.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace gt {
namespace {

struct WavFile {
    std::FILE *file{};
    std::uint32_t data_bytes{};
};

void put32(std::FILE *f, std::uint32_t v) { std::fwrite(&v, 4u, 1u, f); }  // the WAV fields are little endian, like the host
void put16(std::FILE *f, std::uint16_t v) { std::fwrite(&v, 2u, 1u, f); }

void write_header(WavFile &wav, std::uint32_t rate) {
    std::fseek(wav.file, 0, SEEK_SET);
    std::fwrite("RIFF", 1u, 4u, wav.file);
    put32(wav.file, 36u + wav.data_bytes);
    std::fwrite("WAVEfmt ", 1u, 8u, wav.file);
    put32(wav.file, 16u);
    put16(wav.file, 1u);       // PCM
    put16(wav.file, 2u);       // stereo
    put32(wav.file, rate);
    put32(wav.file, rate * 4u);
    put16(wav.file, 4u);
    put16(wav.file, 16u);
    std::fwrite("data", 1u, 4u, wav.file);
    put32(wav.file, wav.data_bytes);
    std::fseek(wav.file, 0, SEEK_END);
}

} // namespace

void audio_dump(std::uint32_t channel, std::span<const std::int16_t> pcm, bool stereo, std::uint32_t rate) {
    static const char *dir = std::getenv("GT_AUDIO_DUMP");
    static std::array<WavFile, 8> files{};
    if (dir == nullptr || channel >= files.size()) return;
    WavFile &wav = files[channel];
    if (wav.file == nullptr) {
        std::filesystem::create_directories(dir);
        const std::string name = (std::filesystem::path(dir) / ("channel_" + std::to_string(channel) + ".wav")).string();
        wav.file = std::fopen(name.c_str(), "wb+");
        if (wav.file == nullptr) return;
        write_header(wav, rate);
    }
    if (stereo) {
        std::fwrite(pcm.data(), sizeof(std::int16_t), pcm.size(), wav.file);
        wav.data_bytes += static_cast<std::uint32_t>(pcm.size() * sizeof(std::int16_t));
    } else {
        for (const std::int16_t sample : pcm) {
            std::fwrite(&sample, sizeof sample, 1u, wav.file);
            std::fwrite(&sample, sizeof sample, 1u, wav.file);
        }
        wav.data_bytes += static_cast<std::uint32_t>(pcm.size() * 2u * sizeof(std::int16_t));
    }
    write_header(wav, rate);  // keeps the file valid whenever the run stops
}

} // namespace gt
