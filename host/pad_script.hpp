// Scripted pad presses (GT_PAD): host/pad_script.cpp (syntax and sequencing: host/pad_sequence.hpp).
#pragma once

#include <cstdint>

namespace psprecomp {
class Runtime;
}

namespace gt {

// One vblank wait passed: advances the script (a WAIT reads guest memory through `runtime`).
void pad_script_tick(psprecomp::Runtime &runtime);
std::uint32_t scripted_buttons();  // PSP button bits pressed by the script now
// The game seeked a file to the absolute byte `offset`: the event `WAIT:seek=<offset>` waits for. The game reads
// GT.VOL entries (a page project, an image, a track) at fixed offsets, so a seek says which entry it is loading.
void pad_script_note_seek(std::uint64_t offset);
std::uint64_t pad_script_vblanks();  // vblank waits so far: the clock GT_PAD times are written in

} // namespace gt
