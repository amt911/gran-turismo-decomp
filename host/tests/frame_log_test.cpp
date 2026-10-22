// Unit tests for gt::FrameLog (host/frame_log.cpp). Plain executable, run by CTest.
#include "frame_log.hpp"

#include <cstdio>
#include <sstream>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main() {
    gt::FrameLog log;
    log.add_present({1000, 2, 33366, true, true, false, 450});
    log.add_vblank({900, 1, 16683, -250, 12000, 0, false});
    log.add_vblank({17500, 2, 33366, 9000, 0, 2, true});
    log.add_present({20000, 3, 50049, false, false, true, 30});
    std::ostringstream out;
    log.write(out);
    const std::string expected =
        "# gt-frame-log v1\n"
        "V 900 1 16683 -250 12000 0 0\n"
        "P 1000 2 33366 1 1 0 450\n"
        "V 17500 2 33366 9000 0 2 1\n"
        "P 20000 3 50049 0 0 1 30\n";
    check(out.str() == expected, "rows are written in wall-clock order in the documented format");
    gt::FrameLog shown;
    shown.add_present({1000, 2, 33366, true, true, false, 450});
    shown.add_display(21000, 7);
    std::ostringstream screen;
    shown.write(screen);
    check(screen.str() == "# gt-frame-log v1\nP 1000 2 33366 1 1 0 450\nD 21000 7\n",
          "display rows follow as D wall_us present_id");
    check(gt::frame_log() == nullptr, "no GT_FRAME_LOG: the process log is off");
    return failures == 0 ? 0 : 1;
}
