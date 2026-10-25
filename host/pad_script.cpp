// GT_PAD="<vblank>:<BUTTON>[:<frames>][,...]" (e.g. "2700:START,3000:CROSS:600"): scripted PSP pad
// presses, each held for <frames> vblank waits (default 6), counted from the first one. The kernel
// ORs them into the host's input, so they work in gt_native (no other input) and gt_play (added to
// the keyboard/gamepad).
//
// A step can also wait for the game instead of a time: `WAIT:<signal>=<value>[:<timeout>]` (syntax and
// sequencing in pad_sequence.hpp). Signals: guest memory words (`read_signal`) and the disc files the game opens
// (`WAIT:seek=<offset>`, from the kernel's sceIoLseek; GT_IO_LOG=1 lists them with the vblank); a wait that is not
// satisfied within its timeout ends the run with exit code 97 and a message, never a silent miss.
//
// GT_PAD_LOG=1 prints each satisfied wait and the end of the script on stderr.
// GT_RAM_DUMP="<dir>:<vblank>[,<vblank>...]" writes the guest RAM (0x08800000..0x0A000000) at those vblanks, a
// debugging aid to find the words that identify a screen (game data: keep the directory out of git).
#include "pad_script.hpp"

#include "pad_sequence.hpp"
#include "psprecomp/runtime.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace gt {
namespace {

// Vblank waits so far: ticked by the main thread, read by frame dumps on the GE worker thread too.
std::atomic<std::uint64_t> g_vblanks{0u};
std::atomic<std::uint32_t> g_buttons{0u};
std::atomic<std::uint64_t> g_pad_reads{0u};
std::atomic<std::uint64_t> g_done_at{0u};  // the vblank the script finished at (0: not yet)

struct Script {
    PadSequencer sequencer;
    bool logged_done{false};
    std::uint64_t done_at{0u};  // the vblank the script was first seen done (GT_PAD_EXIT_AFTER)
    std::string last_message;
};

Script &script() {
    static Script instance = [] {
        const char *text = std::getenv("GT_PAD");
        PadParse parsed = parse_pad_script(text == nullptr ? "" : text);
        if (!parsed.error.empty()) {
            std::fprintf(stderr, "GT_PAD: %s\n", parsed.error.c_str());
            std::fflush(stderr);
            std::_Exit(97);
        }
        return Script{PadSequencer(std::move(parsed.steps)), false, 0u, {}};
    }();
    return instance;
}

// Level signals: `word@0xADDR`, `half@0xADDR`, `byte@0xADDR` (guest address, little endian).
bool read_signal(psprecomp::Runtime &runtime, const std::string &signal, std::uint32_t &value) {
    const auto at = signal.find('@');
    if (at == std::string::npos) return false;
    const std::string kind = signal.substr(0u, at);
    char *end = nullptr;
    const auto address = static_cast<std::uint32_t>(std::strtoul(signal.c_str() + at + 1u, &end, 0));
    if (*end != '\0') return false;
    const std::size_t width = kind == "word" ? 4u : kind == "half" ? 2u : kind == "byte" ? 1u : 0u;
    if (width == 0u || !runtime.memory().contains(address, width)) return false;
    value = width == 4u ? runtime.memory().load32(address) : width == 2u ? runtime.memory().load16(address)
                                                                         : runtime.memory().load8(address);
    return true;
}

// Event signal `seek`: the absolute file seeks the game made (vblank, offset), written by the kernel's sceIoLseek
// and read by the tick, both on the guest thread. The list grows by a few thousand entries per hour of play.
std::vector<std::pair<std::uint64_t, std::uint64_t>> &seeks() {
    static std::vector<std::pair<std::uint64_t, std::uint64_t>> events;
    return events;
}

bool check_wait(psprecomp::Runtime &runtime, const PadStep &wait, std::uint64_t since, std::string &observed) {
    if (wait.signal == "seek") {
        std::size_t seen = 0u;
        for (const auto &[vblank, offset] : seeks()) {
            if (vblank < since) continue;
            ++seen;
            if ((offset == wait.value) == wait.equal) return true;
        }
        char hex[16];
        std::snprintf(hex, sizeof hex, "%x", wait.value);
        observed = std::to_string(seen) + " seeks since vblank " + std::to_string(since) + ", none to 0x" + hex;
        return false;
    }
    std::uint32_t value = 0u;
    if (!read_signal(runtime, wait.signal, value)) {
        observed = wait.signal + " is unreadable";
        return false;
    }
    observed = wait.signal + " is " + std::to_string(value);
    return (value == wait.value) == wait.equal;
}

void ram_dump(psprecomp::Runtime &runtime, std::uint64_t vblank) {
    static const struct Plan {
        std::string directory;
        std::set<std::uint64_t> at;
    } plan = [] {
        Plan plan;
        const char *text = std::getenv("GT_RAM_DUMP");
        if (text == nullptr) return plan;
        const std::string spec = text;
        const auto colon = spec.find(':');
        if (colon == std::string::npos) return plan;
        plan.directory = spec.substr(0u, colon);
        std::string number;
        for (std::size_t i = colon + 1u; i <= spec.size(); ++i) {
            if (i == spec.size() || spec[i] == ',') {
                if (!number.empty()) plan.at.insert(std::strtoull(number.c_str(), nullptr, 10));
                number.clear();
            } else {
                number.push_back(spec[i]);
            }
        }
        return plan;
    }();
    if (plan.at.count(vblank) == 0u) return;
    constexpr std::uint32_t kBase = 0x08800000u, kEnd = 0x0A000000u;
    const std::uint8_t *bytes = runtime.memory().raw_pointer(kBase, kEnd - kBase);
    if (bytes == nullptr) return;
    std::filesystem::create_directories(plan.directory);
    std::ofstream out(std::filesystem::path(plan.directory) / ("ram_" + std::to_string(vblank) + ".bin"), std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes), static_cast<std::streamsize>(kEnd - kBase));
    std::fprintf(stderr, "[ram-dump] vblank %llu\n", static_cast<unsigned long long>(vblank));
}

} // namespace

void pad_script_tick(psprecomp::Runtime &runtime) {
    const std::uint64_t now = g_vblanks.fetch_add(1u, std::memory_order_relaxed) + 1u;
    ram_dump(runtime, now);
    Script &s = script();
    const auto state = s.sequencer.tick(now, [&runtime](const PadStep &wait, std::uint64_t since, std::string &observed) {
        return check_wait(runtime, wait, since, observed);
    });
    static PressLatch latch;
    g_buttons.store(latch.update(s.sequencer.buttons(), g_pad_reads.load(std::memory_order_relaxed)),
                    std::memory_order_relaxed);
    static const bool log = std::getenv("GT_PAD_LOG") != nullptr;
    if (state == PadSequencer::State::TimedOut) {
        std::fprintf(stderr, "[pad] %s\n", s.sequencer.message().c_str());
        kernel_thread_report();
        std::fflush(stderr);
        std::_Exit(97);
    }
    if (log && s.sequencer.message() != s.last_message) {
        s.last_message = s.sequencer.message();
        std::fprintf(stderr, "[pad] %s\n", s.last_message.c_str());
    }
    if (log && state == PadSequencer::State::Done && !s.logged_done) {
        s.logged_done = true;
        std::fprintf(stderr, "[pad] script done at vblank %llu\n", static_cast<unsigned long long>(now));
    }
    // GT_PAD_EXIT_AFTER=<vblanks>: the run ends that long after the script is done ("Runtime stopped: Pad script
    // done", an expected end like the wall-clock limit), so a route covers the same game whatever the machine's load.
    if (state == PadSequencer::State::Done && s.done_at == 0u) {
        s.done_at = now;
        g_done_at.store(now, std::memory_order_relaxed);
    }
    static const char *exit_after = std::getenv("GT_PAD_EXIT_AFTER");
    if (exit_after != nullptr && state == PadSequencer::State::Done) {
        if (now >= s.done_at + std::strtoull(exit_after, nullptr, 10) && !runtime.stopped()) runtime.stop("Pad script done");
    }
}

void pad_script_note_seek(std::uint64_t offset) { seeks().emplace_back(g_vblanks.load(std::memory_order_relaxed), offset); }

std::uint64_t pad_script_done_vblank() { return g_done_at.load(std::memory_order_relaxed); }

std::uint64_t pad_script_vblanks() { return g_vblanks.load(std::memory_order_relaxed); }

std::uint32_t scripted_buttons() {  // one pad read by the game (PressLatch counts them)
    g_pad_reads.fetch_add(1u, std::memory_order_relaxed);
    return g_buttons.load(std::memory_order_relaxed);
}

} // namespace gt
