// gt_play's own host hooks: setting up lcs-recomp's real backends (MIT, compiled from the pinned
// checkout) in the order its LCSNative main uses, and the few parts of its host interface GT must not
// take from the LCS implementation. Design: docs/superpowers/specs/2026-10-10-playable-window-design.md.

#include "display_window.hpp"
#include "frontend.hpp"
#include "ge_gpu_backend.hpp"
#include "lcs_audio_output.hpp"
#include "lcs_lang.hpp"
#include "lcs_render_config.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace lcs {

// lcs_lang.cpp patches LCS's text tables in guest memory: never on GT.
void lcs_install_lang(psprecomp::Runtime &) {}

} // namespace lcs

// Settings come from gt_play.ini next to the executable (config/gt_play.ini, copied by CMake;
// PSPRECOMP_CONFIG overrides; lcs-recomp's format).
void gt::frontend_init(const std::filesystem::path &executable_directory) {
    const std::string config = (executable_directory / "gt_play.ini").string();
    setenv("PSPRECOMP_CONFIG", config.c_str(), 0);
    const std::string saves = (executable_directory / "savedata").string();  // kept between sessions
    setenv("GT_SAVEDATA_DIR", saves.c_str(), 0);
    setenv("PSPRECOMP_WINDOW_TITLE", "Gran Turismo", 0);  // lcs-recomp's default names LCS (our patch 0016)
    setenv("GT_FRAMESKIP", "1", 0);  // keep the PSP's pace on slow CPUs (host/gt_kernel.cpp, frameskip_enabled)
    lcs::initialize_lcs_render_configuration(executable_directory);
    for (const std::string &warning : lcs::lcs_render_configuration().warnings)  // lcs-recomp keeps them silent
        std::cerr << "[config] " << warning << "\n";
    lcs::display_window_init();
}

void gt::frontend_attach() {
    std::string error;
    if (!lcs::initialize_ge_gpu_backend(error)) std::cerr << "[ge] GPU backend unavailable: " << error << "\n";
    lcs::display_window_attach_gpu_backend();
}

void gt::frontend_shutdown() { lcs::audio_output_shutdown(); }
