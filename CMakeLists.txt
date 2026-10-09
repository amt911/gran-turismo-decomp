cmake_minimum_required(VERSION 3.20)
project(gran_turismo_decomp LANGUAGES CXX)

# gt_native: GT's EBOOT recompiled by the lcs-recomp framework (tools/src/lcs-recomp, fetched and
# patched by tools/setup.sh), plus a headless host. The generated C++ in recomp/generated/ is derived
# from the user's game and is never committed. Phase 2a: no graphics, audio or input.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(GT_FRAMEWORK_DIR "${CMAKE_SOURCE_DIR}/tools/src/lcs-recomp")
set(GT_GENERATED_DIR "${CMAKE_SOURCE_DIR}/recomp/generated")
set(GT_LCS_HOST_DIR "${GT_FRAMEWORK_DIR}/lcs/host")

if(NOT EXISTS "${GT_FRAMEWORK_DIR}/src/runtime.cpp")
    message(FATAL_ERROR "Recompiler framework missing: run tools/setup.sh")
endif()
if(NOT EXISTS "${GT_GENERATED_DIR}/generated_registry.cpp")
    message(FATAL_ERROR "Generated code missing: run `.venv/bin/python -m tools.recomp` (after tools.extract)")
endif()

# Optimisation for generated units: measured per-unit peak at -O1 with 0x8000 units is ~372 MB.
set(GT_GENERATED_OPT "-O1" CACHE STRING "Optimisation flag for generated units")

add_library(psprecomp_core STATIC
    "${GT_FRAMEWORK_DIR}/src/decoder.cpp"
    "${GT_FRAMEWORK_DIR}/src/deflate.cpp"
    "${GT_FRAMEWORK_DIR}/src/elf32.cpp"
    "${GT_FRAMEWORK_DIR}/src/guest_memory.cpp"
    "${GT_FRAMEWORK_DIR}/src/nid_registry.cpp"
    "${GT_FRAMEWORK_DIR}/src/program_analysis.cpp"
    "${GT_FRAMEWORK_DIR}/src/runtime.cpp"
    "${GT_FRAMEWORK_DIR}/src/sha256.cpp"
)
target_include_directories(psprecomp_core PUBLIC "${GT_FRAMEWORK_DIR}/include")

file(GLOB GT_GENERATED_SOURCES CONFIGURE_DEPENDS "${GT_GENERATED_DIR}/*.cpp")
add_library(gt_generated STATIC ${GT_GENERATED_SOURCES})
target_include_directories(gt_generated PRIVATE "${GT_GENERATED_DIR}")
target_link_libraries(gt_generated PUBLIC psprecomp_core)
target_compile_options(gt_generated PRIVATE ${GT_GENERATED_OPT} -w)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswresample libswscale)
find_package(Threads REQUIRED)

# Kernel/IO/media HLE: host/gt_kernel.cpp, our copy of lcs-recomp's lcs_profile.cpp (MIT, notice kept)
# extended for GT. SAS, media decoding and GE list walking compile from the pinned checkout (MIT, not
# vendored). Rendering, audio output, window, menu and language hooks are headless stubs of ours.
# Decompiled functions (src/) register themselves with GT_DECOMP; compiled straight into gt_native so the
# static registrars are never dropped by the linker.
file(GLOB_RECURSE GT_DECOMP_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(gt_native
    host/main.cpp
    host/diff_harness.cpp
    host/diff_compare.cpp
    host/probes.cpp
    host/psmf.cpp
    host/psmf_hle.cpp
    host/ps_demux.cpp
    host/movie_decoder.cpp
    ${GT_DECOMP_SOURCES}
    host/headless_backends.cpp
    host/gt_kernel.cpp
    "${GT_LCS_HOST_DIR}/lcs_sas.cpp"
    "${GT_LCS_HOST_DIR}/lcs_media_decoder.cpp"
    "${GT_LCS_HOST_DIR}/lcs_ge_exec.cpp"
    "${GT_LCS_HOST_DIR}/ge_renderer.cpp"
)
target_include_directories(gt_native PRIVATE host "${GT_LCS_HOST_DIR}")
target_link_libraries(gt_native PRIVATE gt_generated psprecomp_core PkgConfig::FFMPEG Threads::Threads)

# Host unit tests (plain executables run by CTest): `ctest --test-dir build`.
enable_testing()
add_executable(host_tests host/tests/diff_compare_test.cpp host/diff_compare.cpp)
target_include_directories(host_tests PRIVATE host)
add_test(NAME diff_compare COMMAND host_tests)
add_executable(psmf_tests host/tests/psmf_test.cpp host/psmf.cpp)
target_include_directories(psmf_tests PRIVATE host)
add_test(NAME psmf COMMAND psmf_tests)
add_executable(ps_demux_tests host/tests/ps_demux_test.cpp host/ps_demux.cpp)
target_include_directories(ps_demux_tests PRIVATE host)
add_test(NAME ps_demux COMMAND ps_demux_tests)
