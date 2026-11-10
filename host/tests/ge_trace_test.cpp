// F8 trace (host/ge_trace.hpp): after request_ge_trace(), every list of the next vblank that submits one is written with
// its detail to ge_trace_vblank<N>.txt in the working directory; the following vblank closes the file and traces nothing.
#include "ge_trace.hpp"

#include "psprecomp/guest_memory.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
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

std::string read(const std::filesystem::path &path) {
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}
} // namespace

int main() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "gt_ge_trace_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::filesystem::current_path(dir);

    psprecomp::GuestMemory memory;
    constexpr std::uint32_t kList = 0x08900000u;
    memory.store32(kList, 0x21000001u);       // blend on
    memory.store32(kList + 4u, 0x0F000000u);  // FINISH

    gt::trace_ge_list(memory, kList, 5u);  // nothing requested: no file
    check(!std::filesystem::exists("ge_trace_vblank5.txt"), "no request, no trace file");

    gt::request_ge_trace();
    gt::trace_ge_list(memory, kList, 7u);
    gt::trace_ge_list(memory, kList + 0u, 7u);  // a second list in the same vblank goes to the same file
    gt::trace_ge_list(memory, kList, 8u);       // the next vblank closes it
    const std::string text = read("ge_trace_vblank7.txt");
    std::size_t lists = 0u;
    for (std::size_t at = text.find("[ge] list"); at != std::string::npos; at = text.find("[ge] list", at + 1u)) ++lists;
    check(lists == 2u, "both lists of vblank 7 traced");
    check(!std::filesystem::exists("ge_trace_vblank8.txt"), "one request traces one vblank");

    gt::request_ge_trace();
    gt::trace_ge_list(memory, kList, 12u);
    gt::trace_ge_list(memory, kList, 13u);
    check(std::filesystem::exists("ge_trace_vblank12.txt"), "a second press traces again");

    // The detail lists every state command that changed since the previous prim (here the global ambient alpha 0x5D,
    // which no fixed field shows), and not the unchanged ones again.
    constexpr std::uint32_t kStates = 0x08901000u;
    const std::uint32_t words[] = {0x21000001u, 0x5D000040u, 0x04030003u, 0x5D000080u, 0x04030003u, 0x0F000000u};
    for (std::uint32_t i = 0; i < 6u; ++i) memory.store32(kStates + i * 4u, words[i]);
    gt::request_ge_trace();
    gt::trace_ge_list(memory, kStates, 20u);
    gt::trace_ge_list(memory, kStates, 21u);
    const std::string states = read("ge_trace_vblank20.txt");
    const std::size_t first = states.find("[ge]   set"), second = states.find("[ge]   set", first + 1u);
    check(first != std::string::npos && second != std::string::npos, "a set line before each prim");
    if (first != std::string::npos && second != std::string::npos) {
        const std::string one = states.substr(first, states.find('\n', first) - first);
        const std::string two = states.substr(second, states.find('\n', second) - second);
        check(one.find("21=000001") != std::string::npos && one.find("5d=000040") != std::string::npos, "first set line has 21 and 5d");
        check(two.find("5d=000080") != std::string::npos, "second set line has the new 5d");
        check(two.find("21=") == std::string::npos, "an unchanged command is not listed again");
    }

    std::filesystem::current_path(std::filesystem::temp_directory_path());
    std::filesystem::remove_all(dir);
    if (failures == 0) std::puts("ge_trace: all checks passed");
    return failures == 0 ? 0 : 1;
}
