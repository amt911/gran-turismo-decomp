// Which presents a frame dump takes (host/frame_dump.cpp): the first present of each bucket of N vblanks, N = GT_FRAME_EVERY.
// Decided by the vblank the present was requested at, not by how many presents came before: a dropped present (GE worker busy)
// shifts a count, so two runs of one pad route would dump disjoint vblanks and no frame would pair up (the checkers pair by
// present_vblank). Not "vblank % N == 0" either: a 30 fps phase presents on odd vblanks only and the modulo would never fire.
#pragma once

#include <cstdint>

namespace gt {

struct DumpBucket {
    std::uint64_t last{~std::uint64_t{0}};

    // True when the present at `present_vblank` is the first of its bucket (and remembers it).
    bool due(std::uint64_t present_vblank, std::uint64_t every) {
        const std::uint64_t bucket = present_vblank / (every == 0u ? 1u : every);
        if (bucket == last) return false;
        last = bucket;
        return true;
    }
};

} // namespace gt
