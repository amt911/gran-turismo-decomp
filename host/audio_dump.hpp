// GT_AUDIO_DUMP=<dir>: every PCM buffer the game outputs (sceAudioOutput*) appended to
// <dir>/channel_N.wav, 16-bit stereo at the channel's rate. Game output: keep it out of git.
#pragma once

#include <cstdint>
#include <span>

namespace gt {

void audio_dump(std::uint32_t channel, std::span<const std::int16_t> pcm, bool stereo, std::uint32_t rate);

} // namespace gt
