// Unit tests for lcs::AudioMixer's drift handling (lcs-recomp's lcs_audio_mixer.hpp, GT patch 0055). Plain executable,
// run by CTest.
#include "lcs_audio_mixer.hpp"

#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// A device that plays in real time: queued blocks drain at 44100 frames per simulated second while playing.
struct FakeDevice {
    std::vector<std::int16_t> block;
    std::vector<std::int16_t> played;  // left channel of every committed block
    std::size_t committed{};
    double drained{};
    bool playing{};
    std::size_t max_outstanding{};

    bool ensure_open(lcs::AudioMixer &mixer) {
        if (!mixer.opened()) mixer.note_device_opened();
        return true;
    }
    std::size_t outstanding_blocks() const { return committed - static_cast<std::size_t>(drained); }
    bool begin_block(std::int16_t *&samples) {
        block.assign(lcs::AudioMixer::kBlockSamples, 0);
        samples = block.data();
        return true;
    }
    bool commit_block() {
        for (std::size_t frame = 0u; frame < lcs::AudioMixer::kBlockFrames; ++frame) played.push_back(block[frame * 2u]);
        ++committed;
        max_outstanding = std::max(max_outstanding, outstanding_blocks());
        return true;
    }
    void resume_playback() { playing = true; }
    void pause_playback() { playing = false; }
    void close_device(bool) {}
    void run_for(double seconds) {
        if (!playing) return;
        drained = std::min(static_cast<double>(committed),
                           drained + seconds * lcs::AudioMixer::kSampleRate / lcs::AudioMixer::kBlockFrames);
    }
};

// The game submits a 44.1 kHz mono ramp (+7 per frame, wrapping) in 10 ms chunks of guest time, with guest time
// running `speed` times the wall clock (the display lock runs it at 60.05/59.94 = 1.0018).
struct Run {
    std::vector<std::int16_t> played;
    std::size_t max_outstanding{};
};
Run run(double speed, double seconds) {
    lcs::AudioMixer mixer;
    FakeDevice device;
    std::vector<std::int16_t> chunk(441);
    std::uint64_t frame = 0u;
    double next_chunk_guest_us = 0.0;
    for (std::uint64_t wall_us = 0u; wall_us < static_cast<std::uint64_t>(seconds * 1e6); wall_us += 1000u) {
        device.run_for(0.001);
        const double guest_us = static_cast<double>(wall_us) * speed;
        while (next_chunk_guest_us <= guest_us) {
            for (auto &sample : chunk) sample = static_cast<std::int16_t>(static_cast<int>((frame++ * 7u) % 20000u) - 10000);
            const auto start = static_cast<std::uint64_t>(next_chunk_guest_us);
            mixer.submit(chunk, 441u, false, 0x8000u, 0x8000u, 44100u, 0u, start, start, device);
            next_chunk_guest_us += 10000.0;
        }
        mixer.advance(static_cast<std::uint64_t>(guest_us), device);
    }
    return {device.played, device.max_outstanding};
}

// A discarded block is a forward jump of 512 frames in the ramp; the ramp's own wrap jumps backwards.
std::size_t forward_jumps(const std::vector<std::int16_t> &played, std::size_t from) {
    std::size_t jumps = 0u;
    for (std::size_t i = from + 1u; i < played.size(); ++i) jumps += played[i] - played[i - 1u] > 100;
    return jumps;
}

// Frames of the ramp advanced per frame played, over the last `frames` frames (1.0: original pitch).
double rate(const std::vector<std::int16_t> &played, std::size_t frames) {
    double advance = 0.0;
    for (std::size_t i = played.size() - frames; i < played.size(); ++i) {
        const int step = played[i] - played[i - 1u];
        if (step > 0 && step < 100) advance += step;
        else advance += 7.0;  // a wrap: count the frame at the ramp's pace
    }
    return advance / 7.0 / static_cast<double>(frames);
}
} // namespace

int main() {
    {  // guest time at the wall clock's pace: every frame played once, nothing skipped, original pitch
        const Run steady = run(1.0, 30.0);
        check(steady.played.size() > 44100u * 25u, "steady: plays");
        check(forward_jumps(steady.played, 44100u) == 0u, "steady: nothing skipped");
        check(std::abs(rate(steady.played, 44100u * 10u) - 1.0) < 0.0005, "steady: original pitch");
    }
    {  // guest time 0.18 % fast (locked to a 60.05 Hz screen): no block thrown away, the extra absorbed smoothly
        const Run fast = run(60.05 / 59.94, 60.0);
        check(forward_jumps(fast.played, 44100u) == 0u, "fast guest: no block thrown away");
        check(std::abs(rate(fast.played, 44100u * 20u) - 60.05 / 59.94) < 0.0005, "fast guest: played at its pace");
        check(fast.max_outstanding <= lcs::AudioMixer::kDefaultPrebufferBlocks + 2u, "fast guest: latency bounded");
    }
    if (failures == 0) std::puts("audio_mixer: all tests passed");
    return failures == 0 ? 0 : 1;
}
