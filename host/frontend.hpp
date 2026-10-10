// The host frontend: what differs between gt_native (headless_backends.cpp: nothing to set up) and
// gt_play (play_backends.cpp: configuration, SDL window, GE GPU backend, audio). main.cpp calls these
// around lcs::install_profile, in lcs-recomp's order.
#pragma once

#include <filesystem>

namespace gt {

void frontend_init(const std::filesystem::path &executable_directory);  // before install_profile
void frontend_attach();                                                  // after install_profile
void frontend_shutdown();                                                // after the run

} // namespace gt
