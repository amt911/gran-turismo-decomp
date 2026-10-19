// The GT_PAD script as data: parsing and sequencing, with no guest memory and no globals, so CTest can drive it
// (host/tests/pad_sequence_test.cpp). host/pad_script.cpp wires it to the vblank clock and to guest memory.
//
// Syntax: a comma-separated list of items (whitespace, newlines included, is ignored).
//   <vblank>:<BUTTON>[:<frames>]        press BUTTON at <vblank>, hold it <frames> vblank waits (default 6)
//   WAIT:<signal>=<value>[:<timeout>]   do not go on until the signal holds (`!=` for "differs from"); fails
//                                       the run loudly after <timeout> vblank waits (default 12000, ~100 s)
// Times are absolute until the first WAIT; after a WAIT is satisfied they count from the vblank where it was,
// until the next one. A WAIT starts looking only when the presses before it have been released.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace gt {

struct PadStep {
    enum class Kind { Press, Wait };
    Kind kind{Kind::Press};
    // Press
    std::uint64_t vblank{};
    std::uint32_t buttons{};
    std::uint64_t frames{6u};
    // Wait
    std::string signal;
    std::uint32_t value{};
    bool equal{true};
    std::uint64_t timeout{12000u};
};

struct PadParse {
    std::vector<PadStep> steps;
    std::string error;  // empty when the text parsed
};

std::uint32_t pad_button_bits(const std::string &name);  // 0 for an unknown name
PadParse parse_pad_script(const std::string &text);

// Does a wait's condition hold now? `since` is the vblank the current segment started at (the last satisfied WAIT):
// event signals ("the game opened file X") count only what happened from then on. `observed` says what was seen,
// for the failure message. A signal that can't be read does not hold.
using PadChecker = std::function<bool(const PadStep &wait, std::uint64_t since, std::string &observed)>;

class PadSequencer {
public:
    enum class State { Running, Done, TimedOut };

    explicit PadSequencer(std::vector<PadStep> steps);

    // Advances the script to vblank `now` (called once per vblank wait, `now` increasing by one).
    State tick(std::uint64_t now, const PadChecker &check);

    std::uint32_t buttons() const { return buttons_; }  // pressed at the last tick
    State state() const { return state_; }
    const std::string &message() const { return message_; }  // the last event: a wait satisfied, a timeout, done

private:
    struct Active {
        std::uint64_t start;
        std::uint64_t end;
        std::uint32_t buttons;
    };
    std::vector<PadStep> steps_;
    std::size_t cursor_{};
    std::uint64_t base_{};
    std::uint64_t segment_end_{};
    bool waiting_{};
    std::uint64_t wait_began_{};
    std::vector<Active> active_;
    std::uint32_t buttons_{};
    State state_{State::Running};
    std::string message_;
};

} // namespace gt
