// The host frontend: what differs between gt_native (headless_backends.cpp: nothing to set up) and
// gt_play (play_backends.cpp: configuration, SDL window, GE GPU backend, audio). main.cpp calls these
// around lcs::install_profile, in lcs-recomp's order.
#pragma once

#include <filesystem>
#include <string>

namespace psprecomp {
class Runtime;
}

namespace gt {

void frontend_init(const std::filesystem::path &executable_directory);  // before install_profile
void frontend_attach();                                                  // after install_profile
void frontend_shutdown();                                                // after the run
// After the decompiled functions are installed: gt_play loads native mods (host/mod_loader.hpp); gt_native never does.
// `allowed` is false for --decomp diff and synthetic cases (the gate must compare the unmodded game). `fatal` is set when
// startup must stop (two mods hooked one address).
void frontend_load_mods(psprecomp::Runtime &runtime, const char *argv0, bool allowed, std::string &fatal);

} // namespace gt
