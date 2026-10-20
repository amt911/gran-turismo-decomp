// GT_PAD parser and sequencer: see pad_sequence.hpp.
#include "pad_sequence.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <utility>

namespace gt {
namespace {

bool parse_number(const std::string &text, std::uint64_t &out) {
    if (text.empty()) return false;
    char *end = nullptr;
    errno = 0;
    out = std::strtoull(text.c_str(), &end, 0);
    return errno == 0 && end != nullptr && *end == '\0';
}

std::string context(const std::string &item, const char *why) { return "'" + item + "': " + why; }

// rest = <signal>(=|!=)<value>[:<timeout>]
bool parse_wait(const std::string &item, const std::string &rest, PadStep &step, std::string &error) {
    std::string condition = rest;
    std::uint64_t timeout = step.timeout;
    if (const auto colon = rest.rfind(':'); colon != std::string::npos) {
        condition = rest.substr(0u, colon);
        if (!parse_number(rest.substr(colon + 1u), timeout) || timeout == 0u) {
            error = context(item, "the timeout must be a positive number of vblank waits");
            return false;
        }
    }
    const auto bang = condition.find("!=");
    const auto equals = condition.find('=');
    if (equals == std::string::npos) {
        error = context(item, "WAIT needs <signal>=<value> or <signal>!=<value>");
        return false;
    }
    const bool differs = bang != std::string::npos && bang + 1u == equals;
    const std::string signal = condition.substr(0u, differs ? bang : equals);
    std::uint64_t value = 0u;
    if (signal.empty() || !parse_number(condition.substr(equals + 1u), value) || value > 0xFFFFFFFFull) {
        error = context(item, "WAIT needs a signal name and a 32-bit number");
        return false;
    }
    step.kind = PadStep::Kind::Wait;
    step.signal = signal;
    step.value = static_cast<std::uint32_t>(value);
    step.equal = !differs;
    step.timeout = timeout;
    return true;
}

// item = <vblank>:<BUTTON>[:<frames>]
bool parse_press(const std::string &item, std::size_t colon, PadStep &step, std::string &error) {
    const std::string rest = item.substr(colon + 1u);
    const auto second = rest.find(':');
    std::uint64_t vblank = 0u, frames = 6u;
    if (!parse_number(item.substr(0u, colon), vblank)) {
        error = context(item, "the time is not a number");
    } else if (second != std::string::npos && !parse_number(rest.substr(second + 1u), frames)) {
        error = context(item, "the hold is not a number");
    } else if ((step.buttons = pad_button_bits(rest.substr(0u, second))) == 0u) {
        error = context(item, "unknown button");
    } else {
        step.vblank = vblank;
        step.frames = frames;
        return true;
    }
    return false;
}

} // namespace

std::uint32_t pad_button_bits(const std::string &name) {
    static const std::pair<const char *, std::uint32_t> kButtons[] = {
        {"SELECT", 0x0001u}, {"START", 0x0008u}, {"UP", 0x0010u}, {"RIGHT", 0x0020u},
        {"DOWN", 0x0040u}, {"LEFT", 0x0080u}, {"L", 0x0100u}, {"R", 0x0200u},
        {"TRIANGLE", 0x1000u}, {"CIRCLE", 0x2000u}, {"CROSS", 0x4000u}, {"SQUARE", 0x8000u},
    };
    for (const auto &[label, bit] : kButtons)
        if (name == label) return bit;
    return 0u;
}

PadParse parse_pad_script(const std::string &text) {
    PadParse parsed;
    std::string item;
    for (std::size_t i = 0;; ++i) {
        if (i == text.size() || text[i] == ',') {
            if (!item.empty()) {
                PadStep step;
                const auto colon = item.find(':');
                bool ok = false;
                if (colon == std::string::npos)
                    parsed.error = context(item, "expected <vblank>:<BUTTON> or WAIT:<signal>=<value>");
                else if (item.compare(0u, colon, "WAIT") == 0)
                    ok = parse_wait(item, item.substr(colon + 1u), step, parsed.error);
                else
                    ok = parse_press(item, colon, step, parsed.error);
                if (!ok) {
                    parsed.steps.clear();
                    return parsed;
                }
                parsed.steps.push_back(std::move(step));
                item.clear();
            }
            if (i == text.size()) break;
        } else if (std::isspace(static_cast<unsigned char>(text[i])) == 0) {  // route files may span lines
            item.push_back(text[i]);
        }
    }
    return parsed;
}

PadSequencer::PadSequencer(std::vector<PadStep> steps) : steps_(std::move(steps)) {}

PadSequencer::State PadSequencer::tick(std::uint64_t now, const PadChecker &check) {
    if (state_ == State::TimedOut) return state_;
    while (cursor_ < steps_.size()) {
        const PadStep &step = steps_[cursor_];
        if (step.kind == PadStep::Kind::Press) {
            const std::uint64_t start = base_ + step.vblank;
            active_.push_back({start, start + step.frames, step.buttons});
            segment_end_ = std::max(segment_end_, start + step.frames);
            ++cursor_;
            continue;
        }
        if (now < segment_end_) break;  // the presses before the wait are still held
        if (!waiting_) {
            waiting_ = true;
            wait_began_ = now;
        }
        std::string observed;
        const std::string what = step.signal + (step.equal ? "=" : "!=") + std::to_string(step.value);
        if (check && check(step, base_, observed)) {
            message_ = "WAIT " + what + " satisfied at vblank " + std::to_string(now) + " (waited " +
                       std::to_string(now - wait_began_) + ")";
            base_ = now;
            segment_end_ = now;
            waiting_ = false;
            ++cursor_;
            continue;
        }
        if (now - wait_began_ >= step.timeout) {
            message_ = "WAIT " + what + " timed out after " + std::to_string(step.timeout) + " vblanks (" +
                       (observed.empty() ? "nothing observed" : observed) + ")";
            state_ = State::TimedOut;
            buttons_ = 0u;
            return state_;
        }
        break;
    }
    buttons_ = 0u;
    for (const Active &press : active_)
        if (now >= press.start && now < press.end) buttons_ |= press.buttons;
    std::erase_if(active_, [now](const Active &press) { return press.end <= now; });
    if (cursor_ == steps_.size() && now >= segment_end_) state_ = State::Done;
    return state_;
}

std::uint32_t PressLatch::update(std::uint32_t scripted, std::uint64_t reads) {
    for (unsigned bit = 0u; bit < 32u; ++bit) {
        const std::uint32_t mask = 1u << bit;
        if ((scripted & mask) != 0u && (previous_ & mask) == 0u) since_[bit] = reads;  // a new press
        if ((held_ & mask) != 0u && (scripted & mask) == 0u && reads - since_[bit] >= min_reads_) held_ &= ~mask;
    }
    held_ |= scripted;
    previous_ = scripted;
    return held_;
}

} // namespace gt
