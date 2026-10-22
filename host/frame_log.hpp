// GT_FRAME_LOG=<file>: per-frame timing of a run, written once at exit (host/frame_log.cpp). Off by default; a disabled
// log costs one branch per call. The format and the reader (python -m tools.frame_pacing) are described in
// tools/frame_pacing.py. Rows hold only timestamps and counters, never game data.
#pragma once

#include <cstdint>
#include <iosfwd>
#include <mutex>
#include <utility>
#include <vector>

namespace gt {

class FrameLog {
public:
    struct Present {
        std::int64_t wall_us;
        std::uint64_t vblank, guest_us;
        bool new_picture, gpu_ready, frameskipped;
        std::int64_t dur_us;
    };
    struct Vblank {
        std::int64_t wall_us;
        std::uint64_t vblank, guest_us;
        std::int64_t lag_us, sleep_us;
        std::uint32_t skipped;
        bool late;
    };

    void add_present(const Present &row);
    void add_vblank(const Vblank &row);
    void add_display(std::int64_t wall_us, std::uint64_t present_id);  // the screen showed present `present_id`
    struct Span {
        char kind;  // 'W' the game thread waiting for the GE worker (tag: the wait site), 'L' the GE executing a list
        std::int64_t begin_us, end_us;
        int tag;
    };
    void add_span(const Span &row);
    void write(std::ostream &out) const;  // "# gt-frame-log v1" then the rows, presents and vblanks interleaved by time

private:
    mutable std::mutex mutex_;
    std::vector<Present> presents_;
    std::vector<Vblank> vblanks_;
    std::vector<std::pair<std::int64_t, std::uint64_t>> displays_;
    std::vector<Span> spans_;
};

// The process-wide log when GT_FRAME_LOG is set (written at exit), else nullptr.
FrameLog *frame_log();
// Microseconds of wall time since the first call (the clock every row uses).
std::int64_t frame_log_now_us();

} // namespace gt
