#include "frame_log.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <ostream>
#include <string>

namespace gt {

void FrameLog::add_present(const Present &row) {
    std::lock_guard lock(mutex_);
    presents_.push_back(row);
}

void FrameLog::add_vblank(const Vblank &row) {
    std::lock_guard lock(mutex_);
    vblanks_.push_back(row);
}

void FrameLog::add_display(std::int64_t wall_us, std::uint64_t present_id) {
    std::lock_guard lock(mutex_);
    displays_.emplace_back(wall_us, present_id);
}
void FrameLog::add_span(const Span &row) {
    std::lock_guard lock(mutex_);
    spans_.push_back(row);
}

void FrameLog::write(std::ostream &out) const {
    std::lock_guard lock(mutex_);
    out << "# gt-frame-log v1\n";
    std::size_t p = 0, v = 0;
    while (p < presents_.size() || v < vblanks_.size()) {
        const bool take_present = v >= vblanks_.size() || (p < presents_.size() && presents_[p].wall_us <= vblanks_[v].wall_us);
        if (take_present) {
            const Present &r = presents_[p++];
            out << "P " << r.wall_us << ' ' << r.vblank << ' ' << r.guest_us << ' ' << r.new_picture << ' ' << r.gpu_ready
                << ' ' << r.frameskipped << ' ' << r.dur_us << '\n';
        } else {
            const Vblank &r = vblanks_[v++];
            out << "V " << r.wall_us << ' ' << r.vblank << ' ' << r.guest_us << ' ' << r.lag_us << ' ' << r.sleep_us << ' '
                << r.skipped << ' ' << r.late << '\n';
        }
    }
    for (const auto &[wall_us, id] : displays_) out << "D " << wall_us << ' ' << id << '\n';
    for (const Span &r : spans_) out << "S " << r.kind << ' ' << r.begin_us << ' ' << r.end_us << ' ' << r.tag << '\n';
}

std::int64_t frame_log_now_us() {
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
}

namespace {
struct Global {
    FrameLog log;
    std::string path;
};
Global *global() {
    static Global *instance = [] () -> Global * {
        const char *path = std::getenv("GT_FRAME_LOG");
        if (path == nullptr || *path == '\0') return nullptr;
        (void)frame_log_now_us();  // start the clock now
        auto *g = new Global{};
        g->path = path;
        // atexit runs before the destructors of statics built earlier, so the GE worker may still add rows: the log's
        // own mutex covers that, and the object is leaked on purpose so a late row never touches a destroyed log.
        std::atexit([] {
            std::ofstream out(global()->path);
            global()->log.write(out);
            if (!out) std::fprintf(stderr, "[frame-log] could not write %s\n", global()->path.c_str());
        });
        return g;
    }();
    return instance;
}
} // namespace

FrameLog *frame_log() {
    Global *g = global();
    return g ? &g->log : nullptr;
}

} // namespace gt
