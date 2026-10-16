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

# Everything both executables share. An OBJECT library, so the GT_DECOMP registrars in src/ are
# linked in whole (a static archive would let the linker drop them).
add_library(gt_host_objs OBJECT
    host/main.cpp
    host/diff_harness.cpp
    host/import_replay.cpp
    host/diff_compare.cpp
    host/probes.cpp
    host/guest_call.cpp
    host/fiber.cpp
    host/kernel_isolation.cpp
    host/ge_trace.cpp
    host/psmf.cpp
    host/psmf_hle.cpp
    host/ps_demux.cpp
    host/movie_decoder.cpp
    host/dirty_pages.cpp
    host/pgf.cpp
    host/font_hle.cpp
    host/atrac_decoder.cpp
    host/audio_dump.cpp
    host/frame_dump.cpp
    host/image_ppm.cpp
    host/pad_script.cpp
    ${GT_DECOMP_SOURCES}
    host/gt_kernel.cpp
    "${GT_LCS_HOST_DIR}/lcs_sas.cpp"
    "${GT_LCS_HOST_DIR}/lcs_media_decoder.cpp"
    host/gt_ge_exec.cpp
    host/ge_state_revision.cpp
    "${GT_LCS_HOST_DIR}/ge_renderer.cpp"
)
target_include_directories(gt_host_objs PRIVATE host include "${GT_LCS_HOST_DIR}")
target_link_libraries(gt_host_objs PRIVATE psprecomp_core PkgConfig::FFMPEG)
# PSP fonts fetched by tools/setup.sh (never committed): the sceLibFont HLE's default directory.
target_compile_definitions(gt_host_objs PRIVATE
    GT_DEFAULT_FONT_DIR="${CMAKE_SOURCE_DIR}/tools/src/ppsspp-fonts/assets/flash0/font")

# gt_native: headless (no window, audio or input; GT_PAD scripts the pad). The differential harness
# and the boot checks run this one.
add_executable(gt_native $<TARGET_OBJECTS:gt_host_objs> host/headless_backends.cpp)
target_include_directories(gt_native PRIVATE host "${GT_LCS_HOST_DIR}")
target_link_libraries(gt_native PRIVATE gt_generated psprecomp_core PkgConfig::FFMPEG Threads::Threads)

# gt_play: the same game with lcs-recomp's real host backends (SDL2 window and input, audio output,
# Vulkan GE backend), compiled from the pinned checkout (MIT, not vendored), plus our hooks for what
# GT must not get from LCS (host/play_backends.cpp). Design: docs/superpowers/specs/
# 2026-10-10-playable-window-design.md.
pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
pkg_check_modules(VULKAN IMPORTED_TARGET vulkan)
find_program(GT_GLSLANG NAMES glslangValidator glslang)
if(SDL2_FOUND AND VULKAN_FOUND AND GT_GLSLANG)
    option(GT_PLAY "Build gt_play (window, input, audio)" ON)
else()
    option(GT_PLAY "Build gt_play (window, input, audio)" OFF)
endif()
if(GT_PLAY)
    set(GT_PLAY_GENERATED "${CMAKE_BINARY_DIR}/play_generated")
    set(GT_GE_SPV_HEADER "${GT_PLAY_GENERATED}/vulkan/ge_spv.hpp")
    add_custom_command(
        OUTPUT "${GT_GE_SPV_HEADER}"
        COMMAND ${CMAKE_COMMAND}
            -DGLSLANG=${GT_GLSLANG}
            -DVERT=${GT_LCS_HOST_DIR}/vulkan/ge.vert
            -DFRAG=${GT_LCS_HOST_DIR}/vulkan/ge.frag
            -DPRESENT_VERT=${GT_LCS_HOST_DIR}/vulkan/present.vert
            -DPRESENT_FRAG=${GT_LCS_HOST_DIR}/vulkan/present.frag
            -DOUT=${GT_GE_SPV_HEADER}
            -P ${GT_LCS_HOST_DIR}/vulkan/embed_spirv.cmake
        DEPENDS ${GT_LCS_HOST_DIR}/vulkan/ge.vert ${GT_LCS_HOST_DIR}/vulkan/ge.frag
                ${GT_LCS_HOST_DIR}/vulkan/present.vert ${GT_LCS_HOST_DIR}/vulkan/present.frag
                ${GT_LCS_HOST_DIR}/vulkan/embed_spirv.cmake
        COMMENT "Compiling Vulkan GE shaders"
        VERBATIM)
    add_executable(gt_pack_font5x7 "${GT_LCS_HOST_DIR}/pack_font5x7.cpp")
    set(GT_FONT_HEADER "${GT_PLAY_GENERATED}/host_font_5x7.hpp")
    add_custom_command(
        OUTPUT "${GT_FONT_HEADER}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GT_PLAY_GENERATED}"
        COMMAND gt_pack_font5x7 "${GT_LCS_HOST_DIR}/font5x7.txt" "${GT_FONT_HEADER}"
        DEPENDS gt_pack_font5x7 "${GT_LCS_HOST_DIR}/font5x7.txt"
        COMMENT "Packing host menu font"
        VERBATIM)
    add_executable(gt_play
        $<TARGET_OBJECTS:gt_host_objs>
        host/play_backends.cpp
        "${GT_LCS_HOST_DIR}/display_window.cpp"
        "${GT_LCS_HOST_DIR}/display_window_sdl.cpp"
        "${GT_LCS_HOST_DIR}/ge_gpu_backend_vulkan.cpp"
        "${GT_LCS_HOST_DIR}/lcs_audio_output.cpp"
        "${GT_LCS_HOST_DIR}/lcs_controls.cpp"
        "${GT_LCS_HOST_DIR}/lcs_display_menu.cpp"
        "${GT_LCS_HOST_DIR}/lcs_fps_overlay.cpp"
        "${GT_LCS_HOST_DIR}/lcs_frame_limit.cpp"
        "${GT_LCS_HOST_DIR}/lcs_menu.cpp"
        "${GT_LCS_HOST_DIR}/lcs_render_config.cpp"
        "${GT_LCS_HOST_DIR}/lcs_runtime_log.cpp"
        "${GT_GE_SPV_HEADER}" "${GT_FONT_HEADER}")
    target_compile_definitions(gt_play PRIVATE LCS_VULKAN_GE_BACKEND=1)
    if(EXISTS "${CMAKE_SOURCE_DIR}/tools/src/Vulkan-Headers/include/vulkan/vulkan.h")  # tools/setup.sh
        target_include_directories(gt_play BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/tools/src/Vulkan-Headers/include")
    endif()
    target_include_directories(gt_play BEFORE PRIVATE "${GT_PLAY_GENERATED}")
    target_include_directories(gt_play PRIVATE host "${GT_LCS_HOST_DIR}")
    target_link_libraries(gt_play PRIVATE gt_generated psprecomp_core PkgConfig::FFMPEG PkgConfig::SDL2
                          PkgConfig::VULKAN Threads::Threads)
    configure_file("${CMAKE_SOURCE_DIR}/config/gt_play.ini" "${CMAKE_BINARY_DIR}/gt_play.ini" COPYONLY)
endif()

# Host unit tests (plain executables run by CTest): `ctest --test-dir build`.
enable_testing()
add_executable(host_tests host/tests/diff_compare_test.cpp host/diff_compare.cpp)
target_include_directories(host_tests PRIVATE host)
add_test(NAME diff_compare COMMAND host_tests)
add_executable(ge_state_revision_tests host/tests/ge_state_revision_test.cpp host/ge_state_revision.cpp)
target_include_directories(ge_state_revision_tests PRIVATE host)
add_test(NAME ge_state_revision COMMAND ge_state_revision_tests)
add_executable(dirty_pages_tests host/tests/dirty_pages_test.cpp host/dirty_pages.cpp)
target_include_directories(dirty_pages_tests PRIVATE host)
target_link_libraries(dirty_pages_tests PRIVATE Threads::Threads)
add_test(NAME dirty_pages COMMAND dirty_pages_tests)
add_executable(import_replay_tests host/tests/import_replay_test.cpp host/import_replay.cpp)
target_include_directories(import_replay_tests PRIVATE host)
add_test(NAME import_replay COMMAND import_replay_tests)
add_executable(image_ppm_tests host/tests/image_ppm_test.cpp host/image_ppm.cpp)
target_include_directories(image_ppm_tests PRIVATE host)
add_test(NAME image_ppm COMMAND image_ppm_tests)
add_executable(psmf_tests host/tests/psmf_test.cpp host/psmf.cpp)
target_include_directories(psmf_tests PRIVATE host)
add_test(NAME psmf COMMAND psmf_tests)
add_executable(ps_demux_tests host/tests/ps_demux_test.cpp host/ps_demux.cpp)
target_include_directories(ps_demux_tests PRIVATE host)
add_test(NAME ps_demux COMMAND ps_demux_tests)
add_executable(movie_decoder_tests host/tests/movie_decoder_test.cpp host/movie_decoder.cpp host/ps_demux.cpp)
target_include_directories(movie_decoder_tests PRIVATE host)
target_link_libraries(movie_decoder_tests PRIVATE PkgConfig::FFMPEG)
add_test(NAME movie_decoder COMMAND movie_decoder_tests)

add_executable(internal_resolution_tests host/tests/internal_resolution_test.cpp)
target_include_directories(internal_resolution_tests PRIVATE "${GT_LCS_HOST_DIR}")
add_test(NAME internal_resolution COMMAND internal_resolution_tests)
add_executable(pgf_tests host/tests/pgf_test.cpp host/pgf.cpp)
target_include_directories(pgf_tests PRIVATE host)
add_test(NAME pgf COMMAND pgf_tests)
add_executable(guest_call_tests host/tests/guest_call_test.cpp host/guest_call.cpp host/fiber.cpp host/kernel_isolation.cpp)
target_include_directories(guest_call_tests PRIVATE host)
target_link_libraries(guest_call_tests PRIVATE psprecomp_core)
add_test(NAME guest_call COMMAND guest_call_tests)
add_executable(guest_view_tests host/tests/guest_view_test.cpp)
target_include_directories(guest_view_tests PRIVATE include)
target_link_libraries(guest_view_tests PRIVATE psprecomp_core)
add_test(NAME guest_view COMMAND guest_view_tests)
add_executable(fiber_tests host/tests/fiber_test.cpp host/guest_call.cpp host/fiber.cpp host/kernel_isolation.cpp)
target_include_directories(fiber_tests PRIVATE host)
target_link_libraries(fiber_tests PRIVATE psprecomp_core)
add_test(NAME fiber COMMAND fiber_tests)

# Branch coverage of chosen decompiled sources (tools.decomp_gate coverage): the listed files (relative to the
# repository root) are compiled with LLVM source-based coverage and the executables link its runtime. Empty
# by default; the tool sets it for one run and resets it.
set(GT_COVERAGE_FILES "" CACHE STRING "Sources built with -fprofile-instr-generate -fcoverage-mapping")
if(GT_COVERAGE_FILES)
    foreach(coverage_file ${GT_COVERAGE_FILES})
        set_source_files_properties("${CMAKE_SOURCE_DIR}/${coverage_file}" PROPERTIES
            COMPILE_OPTIONS "-fprofile-instr-generate;-fcoverage-mapping")
    endforeach()
    target_link_options(gt_native PRIVATE -fprofile-instr-generate)
    if(TARGET gt_play)
        target_link_options(gt_play PRIVATE -fprofile-instr-generate)
    endif()
endif()
