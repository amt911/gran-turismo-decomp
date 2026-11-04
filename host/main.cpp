// gt_native — headless boot of Gran Turismo (PSP, UCES-01245) recompiled by the lcs-recomp framework.
//
// Phase 2a scaffolding for the decomp: load the user's decrypted EBOOT at the measured load base,
// register the generated code and the kernel HLE, and run from the module entry. No graphics, audio
// or input; see docs/superpowers/specs/2026-10-09-phase2a-recomp-boot-design.md.

#include "diff_harness.hpp"
#include "frontend.hpp"
#include "import_lists.hpp"
#include "lcs_profile.hpp"
#include "probes.hpp"
#include "psprecomp/common.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/runtime.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

namespace psprecomp {
void register_generated_functions(Runtime &runtime);
} // namespace psprecomp

namespace {

constexpr std::uint32_t kLoadBase = 0x08804000u;  // measured in PPSSPP, docs/FACTS.md § Runtime

struct Options {
    std::filesystem::path game_root = "extracted/iso";
    std::filesystem::path elf = "extracted/EBOOT.elf";
    std::uint64_t max_dispatches = std::numeric_limits<std::uint64_t>::max();
    double max_seconds = 0.0;
    std::filesystem::path trace;  // import-call trace for tools/trace_compare.py; empty = off
    std::uint64_t trace_limit = 200'000u;  // lines written; later calls are only counted
    gt::DecompMode decomp = gt::DecompMode::Off;  // --decomp off|on|diff (docs/agents/decomp-workflow.md)
    unsigned decomp_cases = 32u;                   // diff cases per decompiled function
};

// Import trace: one line per import call, "<index> 0x<stub address>", in call order. The stub
// address is what PPSSPP's "Importing NAME : STUB" log lines name, so both traces can be compared
// without an external NID database. Plain globals because the observer is a function pointer.
std::FILE *g_trace = nullptr;
std::uint64_t g_trace_index = 0u;
bool g_abnormal_stop = false;  // the runtime stopped for a reason other than an expected end
std::uint64_t g_trace_limit = 0u;
std::map<std::pair<std::string, std::uint32_t>, std::uint32_t, std::less<>> g_stub_by_import;

void trace_import(std::string_view library, std::uint32_t nid) {
    gt::note_import();
    if (g_trace == nullptr) return;
    const auto it = g_stub_by_import.find(std::pair<std::string, std::uint32_t>(std::string(library), nid));
    const std::uint32_t stub = it != g_stub_by_import.end() ? it->second : 0u;
    if (g_trace_index < g_trace_limit)
        std::fprintf(g_trace, "%llu 0x%08x\n", static_cast<unsigned long long>(g_trace_index), stub);
    ++g_trace_index;
}

Options parse_options(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--game" && has_value) options.game_root = argv[++i];
        else if (arg == "--elf" && has_value) options.elf = argv[++i];
        else if (arg == "--max-dispatches" && has_value) options.max_dispatches = std::strtoull(argv[++i], nullptr, 0);
        else if (arg == "--max-seconds" && has_value) options.max_seconds = std::strtod(argv[++i], nullptr);
        else if (arg == "--trace" && has_value) options.trace = argv[++i];
        else if (arg == "--trace-limit" && has_value) options.trace_limit = std::strtoull(argv[++i], nullptr, 0);
        else if (arg == "--decomp" && has_value) {
            if (!gt::parse_decomp_mode(argv[++i], options.decomp)) std::cerr << "gt_native: --decomp expects off|on|diff\n";
        }
        else if (arg == "--decomp-cases" && has_value) options.decomp_cases = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 0));
        else std::cerr << "gt_native: ignoring argument \"" << arg << "\"\n";
    }
    return options;
}

std::uint32_t user_arena_start(const psprecomp::Elf32Image &elf) {
    std::uint64_t image_end = 0u;
    for (std::size_t index = 0; index < elf.segments().size(); ++index) {
        const auto &segment = elf.segments()[index];
        if (segment.type != 1u) continue;  // PT_LOAD
        image_end = std::max<std::uint64_t>(image_end, std::uint64_t{elf.segment_runtime_address(index, kLoadBase)} + segment.memory_size);
    }
    return static_cast<std::uint32_t>((image_end + 0xFFu) & ~0xFFull);
}

} // namespace

int main(int argc, char **argv) {
    const Options options = parse_options(argc, argv);
    // Idle software-rasterizer workers park after a short spin instead of lcs-recomp's 262,144 pauses:
    // same frame times, half the CPU, and fewer late vblanks on a power-limited CPU (docs/FINDINGS.md).
    setenv("PSPRECOMP_RASTER_SPINS", "2048", 0);
    // lcs-recomp's static vertex cache hashes every cached draw's vertex bytes on the GE thread each frame to
    // reuse their decode; in GT's races that costs more than decoding them again (race, GPU mode: 77 -> 83
    // vblanks/s without it; docs/FINDINGS.md). PSPRECOMP_GE_VERTEX_CACHE=1 turns it back on.
    setenv("PSPRECOMP_GE_VERTEX_CACHE", "0", 0);
    try {
        auto elf = psprecomp::Elf32Image::from_file(options.elf);
        psprecomp::Runtime runtime;
        runtime.set_game_root(options.game_root);

        const auto stats = elf.load_and_relocate(runtime.memory(), kLoadBase);
        std::cout << "Loaded " << options.elf.string() << " (" << stats.total << " relocations)\n";

        psprecomp::register_generated_functions(runtime);
        std::cout << "Registered " << runtime.function_count() << " generated functions\n";

        std::error_code exe_error;
        gt::frontend_init(argc > 0 ? std::filesystem::absolute(argv[0], exe_error).parent_path()
                                   : std::filesystem::current_path());
        lcs::install_profile(runtime, user_arena_start(elf));
        gt::frontend_attach();
        lcs::set_wall_clock_limit(options.max_seconds);

        const auto module = elf.find_module_info(runtime.memory(), kLoadBase);
        if (!module) throw psprecomp::Error("PSP module info not found after relocation");
        runtime.cpu().set_gpr(28, module->gp);

        // Which imports run in both runs of a case, which are recorded: host/import_lists.hpp.
        const auto &kReplaySafeNids = gt::replay_safe_nids();
        const auto &kRecordableNids = gt::recordable_nids();
        std::vector<gt::ImportStub> import_stubs;
        std::ofstream trace_names;
        if (!options.trace.empty()) trace_names.open(options.trace.string() + ".names", std::ios::trunc);
        for (const auto &import : elf.scan_imports(runtime.memory(), *module)) {
            g_stub_by_import[{import.library, import.nid}] = import.stub_address;
            gt::ImportStub stub;
            stub.address = import.stub_address;
            stub.name = import.library + "::" +
                        runtime.nids().resolve(import.library, import.nid).value_or(psprecomp::hex32(import.nid));
            if (!options.trace.empty()) {  // <trace>.names: what each stub address of the trace is
                trace_names << psprecomp::hex32(import.stub_address) << " " << stub.name << "\n";
            }
            stub.replay_safe = kReplaySafeNids.contains(import.nid);
            if (const auto recordable = kRecordableNids.find(import.nid); recordable != kRecordableNids.end()) {
                stub.recordable = true;
                stub.arg_mask = recordable->second;
            }
            import_stubs.push_back(std::move(stub));
        }
        gt::set_imports(std::move(import_stubs));
        if (!options.trace.empty()) {
            g_trace = std::fopen(options.trace.c_str(), "w");
            if (g_trace == nullptr) throw psprecomp::Error("cannot open trace file " + options.trace.string());
            g_trace_limit = options.trace_limit;
        }
        // One observer for both: the trace (when enabled) and the harness's import counter.
        psprecomp::set_runtime_import_observer(&trace_import);
        gt::install_probes(runtime);  // GT_PROBE=addr,... (debugging aid)
        gt::install_decompiled(runtime, options.decomp, options.decomp_cases);
        {  // native mods (gt_play only; the call is a no-op in gt_native). After install_decompiled so mod hooks wrap decompiled functions too.
            std::string mods_fatal;
            gt::frontend_load_mods(runtime, argc > 0 ? argv[0] : "",
                                   options.decomp != gt::DecompMode::Diff && !gt::synthetic_mode(), mods_fatal);
            if (!mods_fatal.empty()) {
                std::cerr << "gt_play: mod conflict: " << mods_fatal << "\n";
                return 3;
            }
        }
        runtime.cpu().set_gpr(31, 0u);
        runtime.cpu().set_gpr(4, 0u);
        runtime.cpu().set_gpr(5, 0u);

        const std::uint32_t entry = elf.runtime_entry(kLoadBase);
        std::cout << "Running from entry " << psprecomp::hex32(entry) << "\n" << std::flush;
        runtime.run(entry, options.max_dispatches);
        lcs::ge_worker_shutdown();
        if (runtime.stopped()) {
            std::cout << "Runtime stopped: " << runtime.stop_reason() << "\n";
            // Only the wall-clock bound and the game ending its threads are expected ends; any other
            // stop (a decompiled call to a non-function, a deadlock...) cut the run short, and a
            // diff run that stopped early has not compared what it should have.
            const std::string &reason = runtime.stop_reason();
            g_abnormal_stop = !reason.starts_with("Wall-clock limit reached") && reason != "All PSP threads completed" &&
                              reason != "Pad script done" &&  // GT_PAD_EXIT_AFTER
                              reason != "Synthetic cases done";  // GT_SYNTH
        }
        gt::frontend_shutdown();
        psprecomp::set_runtime_import_observer(nullptr);
        if (g_trace != nullptr) {
            std::fclose(g_trace);
            std::cout << "Traced " << g_trace_index << " import calls to " << options.trace.string() << "\n";
        }
    } catch (const std::exception &ex) {
        std::cerr << "gt_native error: " << ex.what() << "\n";
        gt::probe_report();
        const int decomp_status = gt::decomp_report();  // a mismatch (2) wins over the crash (1)
        return decomp_status != 0 ? decomp_status : 1;
    }
    gt::probe_report();
    const int decomp_status = gt::decomp_report();  // a mismatch (2) wins over an early stop (3)
    return decomp_status != 0 ? decomp_status : (g_abnormal_stop ? 3 : 0);
}
