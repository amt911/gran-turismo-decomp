// Unit tests for the GT_PAD parser and sequencer (host/pad_sequence.cpp). Plain executable, run by CTest.
#include "pad_sequence.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <utility>
#include <vector>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

constexpr std::uint32_t kStart = 0x0008u, kCross = 0x4000u, kCircle = 0x2000u;

// A fake guest: level signals are a map the test edits; "seek" is an event signal (the game seeked offset N at a vblank).
struct World {
    std::map<std::string, std::uint32_t> values;
    std::vector<std::pair<std::uint64_t, std::uint32_t>> seeks;
    gt::PadChecker reader() {
        return [this](const gt::PadStep &wait, std::uint64_t since, std::string &observed) {
            if (wait.signal == "seek") {
                observed = "seeks: " + std::to_string(seeks.size());
                for (const auto &[vblank, file] : seeks)
                    if (vblank >= since && (file == wait.value) == wait.equal) return true;
                return false;
            }
            const auto it = values.find(wait.signal);
            if (it == values.end()) return false;
            observed = wait.signal + " is " + std::to_string(it->second);
            return (it->second == wait.value) == wait.equal;
        };
    }
};

// Runs vblanks 1..last and returns the buttons at each.
std::map<std::uint64_t, std::uint32_t> run(gt::PadSequencer &sequencer, World &world, std::uint64_t last) {
    std::map<std::uint64_t, std::uint32_t> pressed;
    const auto reader = world.reader();
    for (std::uint64_t now = 1; now <= last; ++now) {
        sequencer.tick(now, reader);
        pressed[now] = sequencer.buttons();
    }
    return pressed;
}

// A route file (config/pad_routes/*.pad) as the gate hands it to GT_PAD: the `#` lines removed. It must parse and be deterministic: at least
// one WAIT, and no segment (the presses between two waits) longer than kMaxSegment vblanks, which would be a fixed sleep standing in for a signal.
constexpr std::uint64_t kMaxSegment = 6000u;
void check_route(const char *path) {
    std::ifstream in(path);
    check(in.good(), path);
    std::string line, text;
    while (std::getline(in, line))
        if (line.empty() || line[0] != '#') text += line + "\n";
    const auto parsed = gt::parse_pad_script(text);
    check(parsed.error.empty(), (std::string(path) + ": " + parsed.error).c_str());
    std::size_t waits = 0;
    std::uint64_t previous = 0;  // the previous press of this segment (offsets from its wait)
    for (const auto &step : parsed.steps) {
        if (step.kind == gt::PadStep::Kind::Wait) {
            ++waits;
            previous = 0;
            continue;
        }
        if (waits > 0)
            check(step.vblank - previous <= kMaxSegment, (std::string(path) + ": idle gap between presses").c_str());
        previous = step.vblank;
    }
    check(waits > 0, (std::string(path) + ": no WAIT, the route runs on vblank counts").c_str());
}
} // namespace

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) check_route(argv[i]);
    if (argc > 1) return failures == 0 ? 0 : 1;
    {  // the existing syntax parses as before: defaults and an explicit hold
        const auto parsed = gt::parse_pad_script("3300:START,6600:CROSS:600");
        check(parsed.error.empty() && parsed.steps.size() == 2u, "legacy two items");
        check(parsed.steps[0].kind == gt::PadStep::Kind::Press && parsed.steps[0].vblank == 3300u &&
                  parsed.steps[0].buttons == kStart && parsed.steps[0].frames == 6u,
              "legacy default hold is 6");
        check(parsed.steps[1].frames == 600u && parsed.steps[1].buttons == kCross, "legacy explicit hold");
    }
    {  // WAIT items
        const auto parsed = gt::parse_pad_script("WAIT:page=0x1f:500,WAIT:busy!=0,10:CROSS");
        check(parsed.error.empty() && parsed.steps.size() == 3u, "wait items parse");
        check(parsed.steps[0].kind == gt::PadStep::Kind::Wait && parsed.steps[0].signal == "page" &&
                  parsed.steps[0].value == 0x1fu && parsed.steps[0].equal && parsed.steps[0].timeout == 500u,
              "wait = hex with timeout");
        check(!parsed.steps[1].equal && parsed.steps[1].value == 0u && parsed.steps[1].timeout == 12000u,
              "wait != with the default timeout");
        check(parsed.steps[2].vblank == 10u && parsed.steps[2].buttons == kCross, "press after a wait");
    }
    {  // a route file spans lines: whitespace between and inside items is ignored
        const auto parsed = gt::parse_pad_script("3300:START,\n  WAIT:seek=0x10 :200,\n\t5:CROSS\n");
        check(parsed.error.empty() && parsed.steps.size() == 3u && parsed.steps[1].timeout == 200u, "multi-line script");
    }
    {  // a signal name may carry an @address: only the last '=' / '!=' splits
        const auto parsed = gt::parse_pad_script("WAIT:word@0x08C00000=7");
        check(parsed.error.empty() && parsed.steps[0].signal == "word@0x08C00000" && parsed.steps[0].value == 7u,
              "signal with an address");
    }
    {  // malformed items are errors, never silently dropped
        for (const char *bad : {"3300:NOPE", "WAIT:page", "WAIT:=3", "WAIT:page=zz", "WAIT:page=1:x", "abc:START", "3300", "WAIT:page=1:0"}) {
            const auto parsed = gt::parse_pad_script(bad);
            if (parsed.error.empty()) std::fprintf(stderr, "accepted: %s\n", bad);
            check(!parsed.error.empty(), "malformed script rejected");
        }
        check(gt::parse_pad_script("").error.empty() && gt::parse_pad_script("").steps.empty(), "empty script is fine");
    }
    {  // without a WAIT the sequencer is the old absolute-time script
        gt::PadSequencer sequencer(gt::parse_pad_script("5:START:3,20:CROSS").steps);
        World world;
        auto pressed = run(sequencer, world, 30);
        check(pressed[4] == 0u && pressed[5] == kStart && pressed[7] == kStart && pressed[8] == 0u, "START held 5..7");
        check(pressed[19] == 0u && pressed[20] == kCross && pressed[25] == kCross && pressed[26] == 0u, "CROSS 20..25");
        check(sequencer.state() == gt::PadSequencer::State::Done, "done after the last press");
    }
    {  // a WAIT holds the later presses back until the signal is there, and times count from it
        gt::PadSequencer sequencer(gt::parse_pad_script("2:START:2,WAIT:page=3,4:CROSS:2").steps);
        World world;
        const auto reader = world.reader();
        std::map<std::uint64_t, std::uint32_t> pressed;
        for (std::uint64_t now = 1; now <= 40; ++now) {
            if (now == 20) world.values["page"] = 3u;  // the screen arrives at vblank 20
            sequencer.tick(now, reader);
            pressed[now] = sequencer.buttons();
        }
        check(pressed[2] == kStart && pressed[3] == kStart && pressed[4] == 0u, "press before the wait");
        check(pressed[23] == 0u && pressed[24] == kCross && pressed[25] == kCross && pressed[26] == 0u,
              "press 4 vblanks after the wait was satisfied at 20");
        check(sequencer.state() == gt::PadSequencer::State::Done, "done");
    }
    {  // a signal that is already true does not delay anything beyond the previous presses
        gt::PadSequencer sequencer(gt::parse_pad_script("1:START:3,WAIT:page=3,0:CROSS").steps);
        World world;
        world.values["page"] = 3u;
        auto pressed = run(sequencer, world, 10);
        check(pressed[1] == kStart && pressed[3] == kStart, "START held 1..3");
        check(pressed[4] == kCross && pressed[9] == kCross && pressed[10] == 0u, "the wait looks only once START is released, CROSS at 0 after");
    }
    {  // a timeout fails loudly, with the signal in the message
        gt::PadSequencer sequencer(gt::parse_pad_script("WAIT:page=3:10,1:CROSS").steps);
        World world;
        world.values["page"] = 9u;
        const auto reader = world.reader();
        gt::PadSequencer::State state = gt::PadSequencer::State::Running;
        std::uint64_t at = 0;
        for (std::uint64_t now = 1; now <= 50 && state == gt::PadSequencer::State::Running; ++now) {
            state = sequencer.tick(now, reader);
            at = now;
        }
        check(state == gt::PadSequencer::State::TimedOut, "times out");
        check(at == 11u, "after exactly the timeout, counted from the first look");
        check(sequencer.message().find("page is 9") != std::string::npos,
              "message names the signal and its value");
        check(sequencer.buttons() == 0u, "nothing pressed after the failure");
    }
    {  // an unreadable signal keeps waiting, then times out
        gt::PadSequencer sequencer(gt::parse_pad_script("WAIT:gone=1:5").steps);
        World world;
        const auto reader = world.reader();
        gt::PadSequencer::State state = gt::PadSequencer::State::Running;
        for (std::uint64_t now = 1; now <= 10; ++now) state = sequencer.tick(now, reader);
        check(state == gt::PadSequencer::State::TimedOut, "an unreadable signal times out");
    }
    {  // != and two waits in a row
        gt::PadSequencer sequencer(gt::parse_pad_script("WAIT:a!=0,WAIT:b=2,0:CIRCLE:1").steps);
        World world;
        const auto reader = world.reader();
        world.values["a"] = 0u;
        world.values["b"] = 2u;
        for (std::uint64_t now = 1; now <= 5; ++now) sequencer.tick(now, reader);
        check(sequencer.buttons() == 0u && sequencer.state() == gt::PadSequencer::State::Running, "first wait blocks");
        world.values["a"] = 7u;
        sequencer.tick(6, reader);
        check(sequencer.buttons() == kCircle, "both waits pass in the same vblank, the press is at 0");
        sequencer.tick(7, reader);
        check(sequencer.buttons() == 0u && sequencer.state() == gt::PadSequencer::State::Done, "released and done");
    }
    {  // an event signal counts only what happened since the last satisfied wait
        gt::PadSequencer sequencer(gt::parse_pad_script("WAIT:seek=0x10,2:CROSS:1,WAIT:seek=0x10,0:CIRCLE:1").steps);
        World world;
        const auto reader = world.reader();
        world.seeks.push_back({0u, 0x10u});  // before the script: counts for the first wait (since 0)
        sequencer.tick(1, reader);
        sequencer.tick(2, reader);
        sequencer.tick(3, reader);
        check(sequencer.buttons() == kCross, "the first wait took the seek at vblank 0, CROSS 2 later");
        sequencer.tick(4, reader);
        for (std::uint64_t now = 5; now <= 8; ++now) sequencer.tick(now, reader);
        check(sequencer.buttons() == 0u && sequencer.state() == gt::PadSequencer::State::Running,
              "the second wait does not take the seek from before the first wait was satisfied");
        world.seeks.push_back({8u, 0x10u});
        sequencer.tick(9, reader);
        check(sequencer.buttons() == kCircle, "a new seek at 8 satisfies it");
    }
    return failures == 0 ? 0 : 1;
}
