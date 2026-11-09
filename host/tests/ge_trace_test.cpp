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

    std::filesystem::current_path(std::filesystem::temp_directory_path());
    std::filesystem::remove_all(dir);
    if (failures == 0) std::puts("ge_trace: all checks passed");
    return failures == 0 ? 0 : 1;
}
