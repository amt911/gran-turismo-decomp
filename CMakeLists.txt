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

# Profile-guided optimisation (clang): GT_PGO_GENERATE=ON builds an instrumented game whose runs write .profraw files;
# GT_PGO_PROFILE=<merged .profdata> optimises with one. tools/pgo.sh does both. Measured on the laptop in power-saver,
# race route: the game thread 59 % -> 39 % of a core, the GE 508 -> 470 ns per draw (docs/FINDINGS.md, 2026-10-19).
# The profile comes from the user's own runs of the game and stays out of git (recomp/analysis/pgo/).
option(GT_PGO_GENERATE "Instrument the build for profile-guided optimisation (clang)" OFF)
set(GT_PGO_PROFILE "" CACHE FILEPATH "Merged .profdata to optimise with (clang); empty = no PGO")
if(GT_PGO_GENERATE OR GT_PGO_PROFILE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "GT_PGO_GENERATE / GT_PGO_PROFILE need clang (-DCMAKE_CXX_COMPILER=clang++)")
    endif()
endif()
if(GT_PGO_GENERATE AND GT_PGO_PROFILE)
    message(FATAL_ERROR "GT_PGO_GENERATE and GT_PGO_PROFILE are exclusive: generate in one build dir, use in another")
elseif(GT_PGO_GENERATE)
    add_compile_options(-fprofile-instr-generate)
    add_link_options(-fprofile-instr-generate)
elseif(GT_PGO_PROFILE)
    if(NOT EXISTS "${GT_PGO_PROFILE}")
        message(FATAL_ERROR "GT_PGO_PROFILE: ${GT_PGO_PROFILE} does not exist (tools/pgo.sh writes it)")
    endif()
    # A profile from an older build still helps: functions that changed are simply not profiled.
    add_compile_options("-fprofile-instr-use=${GT_PGO_PROFILE}" -Wno-profile-instr-unprofiled
                        -Wno-profile-instr-out-of-date -Wno-backend-plugin)
endif()

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
pkg_check_modules(LIBCRYPTO REQUIRED IMPORTED_TARGET libcrypto)  # AES for the PSP savedata cipher (OpenSSL, system)
find_package(Threads REQUIRED)

# Kernel/IO/media HLE: host/gt_kernel.cpp, our copy of lcs-recomp's lcs_profile.cpp (MIT, notice kept)
# extended for GT. SAS, media decoding and GE list walking compile from the pinned checkout (MIT, not
# vendored). Rendering, audio output, window, menu and language hooks are headless stubs of ours.
# Decompiled functions (src/) register themselves with GT_DECOMP; compiled straight into gt_native so the
# static registrars are never dropped by the linker.
file(GLOB_RECURSE GT_DECOMP_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

# Everything both executables share. An OBJECT library, so the GT_DECOMP registrars in src/ are
# linked in whole (a static archive would let the linker drop them).
# KIRK slot keys for the PSP savedata cipher (spec docs/superpowers/specs/2026-10-23-savedata-routes-design.md): read as
# data from the pinned pspdecrypt checkout tools/setup.sh fetches, checked against config/kirk_slot_keys.sha256, written
# under the build directory only — never committed.
set(GT_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
add_custom_command(
    OUTPUT "${GT_GENERATED_DIR}/kirk_slot_keys.hpp"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GT_GENERATED_DIR}"
    COMMAND "${CMAKE_SOURCE_DIR}/.venv/bin/python" -I tools/savedata.py keys "${GT_GENERATED_DIR}/kirk_slot_keys.hpp"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS tools/savedata.py config/kirk_slot_keys.sha256 tools/src/pspdecrypt/libkirk/kirk_engine.c
    VERBATIM)
add_custom_target(gt_kirk_slot_keys DEPENDS "${GT_GENERATED_DIR}/kirk_slot_keys.hpp")
add_library(gt_host_objs OBJECT
    host/psp_savedata_crypto.cpp
    host/main.cpp
    host/diff_harness.cpp
    host/synth_case.cpp
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
    host/pad_sequence.cpp
    ${GT_DECOMP_SOURCES}
    host/gt_kernel.cpp
    "${GT_LCS_HOST_DIR}/lcs_sas.cpp"
    "${GT_LCS_HOST_DIR}/lcs_media_decoder.cpp"
    host/gt_ge_exec.cpp
    host/ge_state_revision.cpp
    host/vblank_pacer.cpp
    host/display_phase.cpp
    host/frame_log.cpp
    "${GT_LCS_HOST_DIR}/ge_renderer.cpp"
)
target_include_directories(gt_host_objs PRIVATE host include "${GT_LCS_HOST_DIR}" "${GT_GENERATED_DIR}")
target_link_libraries(gt_host_objs PRIVATE psprecomp_core PkgConfig::FFMPEG PkgConfig::LIBCRYPTO)
add_dependencies(gt_host_objs gt_kirk_slot_keys)
# PSP fonts fetched by tools/setup.sh (never committed): the sceLibFont HLE's default directory.
target_compile_definitions(gt_host_objs PRIVATE
    GT_DEFAULT_FONT_DIR="${CMAKE_SOURCE_DIR}/tools/src/ppsspp-fonts/assets/flash0/font")

# gt_native: headless (no window, audio or input; GT_PAD scripts the pad). The differential harness
# and the boot checks run this one.
add_executable(gt_native $<TARGET_OBJECTS:gt_host_objs> host/headless_backends.cpp)
target_include_directories(gt_native PRIVATE host "${GT_LCS_HOST_DIR}")
target_link_libraries(gt_native PRIVATE gt_generated psprecomp_core PkgConfig::FFMPEG PkgConfig::LIBCRYPTO Threads::Threads)

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
    target_link_libraries(gt_play PRIVATE gt_generated psprecomp_core PkgConfig::FFMPEG PkgConfig::LIBCRYPTO PkgConfig::SDL2
                          PkgConfig::VULKAN Threads::Threads)
    # The user's settings live in build/gt_play.ini: copy the template only when there is none, so a reconfigure or a
    # pull that changes config/gt_play.ini doesn't wipe them (keys an older copy lacks take the program's defaults).
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/gt_play.ini")
        file(COPY "${CMAKE_SOURCE_DIR}/config/gt_play.ini" DESTINATION "${CMAKE_BINARY_DIR}")
    endif()
endif()

# Host unit tests (plain executables run by CTest): `ctest --test-dir build`.
enable_testing()
add_executable(host_tests host/tests/diff_compare_test.cpp host/diff_compare.cpp)
target_include_directories(host_tests PRIVATE host)
add_test(NAME diff_compare COMMAND host_tests)
add_executable(ge_state_revision_tests host/tests/ge_state_revision_test.cpp host/ge_state_revision.cpp)
target_include_directories(ge_state_revision_tests PRIVATE host)
add_test(NAME ge_state_revision COMMAND ge_state_revision_tests)
add_executable(vblank_pacer_tests host/tests/vblank_pacer_test.cpp host/vblank_pacer.cpp host/display_phase.cpp)
target_include_directories(vblank_pacer_tests PRIVATE host)
add_test(NAME vblank_pacer COMMAND vblank_pacer_tests)
add_executable(display_phase_tests host/tests/display_phase_test.cpp host/vblank_pacer.cpp host/display_phase.cpp)
target_include_directories(display_phase_tests PRIVATE host)
add_test(NAME display_phase COMMAND display_phase_tests)
add_executable(ge_blend_tests host/tests/ge_blend_test.cpp "${GT_LCS_HOST_DIR}/ge_renderer.cpp" host/headless_backends.cpp)
target_include_directories(ge_blend_tests PRIVATE host "${GT_LCS_HOST_DIR}")
target_link_libraries(ge_blend_tests PRIVATE psprecomp_core)
add_test(NAME ge_blend COMMAND ge_blend_tests)
add_executable(savedata_crypto_tests host/tests/savedata_crypto_test.cpp host/psp_savedata_crypto.cpp)
target_include_directories(savedata_crypto_tests PRIVATE host)
target_link_libraries(savedata_crypto_tests PRIVATE PkgConfig::LIBCRYPTO)
add_test(NAME savedata_crypto COMMAND savedata_crypto_tests)
add_executable(audio_mixer_tests host/tests/audio_mixer_test.cpp)
target_include_directories(audio_mixer_tests PRIVATE "${GT_LCS_HOST_DIR}")
add_test(NAME audio_mixer COMMAND audio_mixer_tests)
add_executable(frame_log_tests host/tests/frame_log_test.cpp host/frame_log.cpp)
target_include_directories(frame_log_tests PRIVATE host)
add_test(NAME frame_log COMMAND frame_log_tests)
add_executable(dirty_pages_tests host/tests/dirty_pages_test.cpp host/dirty_pages.cpp)
target_include_directories(dirty_pages_tests PRIVATE host)
target_link_libraries(dirty_pages_tests PRIVATE Threads::Threads)
add_test(NAME dirty_pages COMMAND dirty_pages_tests)
add_executable(import_replay_tests host/tests/import_replay_test.cpp host/import_replay.cpp)
target_include_directories(import_replay_tests PRIVATE host)
add_test(NAME import_replay COMMAND import_replay_tests)
add_executable(import_lists_tests host/tests/import_lists_test.cpp)
target_include_directories(import_lists_tests PRIVATE host)
add_test(NAME import_lists COMMAND import_lists_tests)
add_executable(image_ppm_tests host/tests/image_ppm_test.cpp host/image_ppm.cpp)
target_include_directories(image_ppm_tests PRIVATE host)
add_test(NAME image_ppm COMMAND image_ppm_tests)
add_executable(psmf_tests host/tests/psmf_test.cpp host/psmf.cpp)
target_include_directories(psmf_tests PRIVATE host)
add_test(NAME psmf COMMAND psmf_tests)
enable_language(C)  # host/tests/mod_api_c_check.c proves the mod SDK header is valid C99
set(CMAKE_C_STANDARD 99)
add_executable(mod_manifest_tests host/tests/mod_manifest_test.cpp host/mod_manifest.cpp host/tests/mod_api_c_check.c)
target_include_directories(mod_manifest_tests PRIVATE host include)
add_test(NAME mod_manifest COMMAND mod_manifest_tests)
add_executable(mod_hooks_tests host/tests/mod_hooks_test.cpp host/mod_hooks.cpp)
target_include_directories(mod_hooks_tests PRIVATE host include)
target_link_libraries(mod_hooks_tests PRIVATE psprecomp_core)
add_test(NAME mod_hooks COMMAND mod_hooks_tests)
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
add_executable(synth_case_tests host/tests/synth_case_test.cpp host/synth_case.cpp)
target_include_directories(synth_case_tests PRIVATE host)
add_test(NAME synth_case COMMAND synth_case_tests)
add_executable(pad_sequence_tests host/tests/pad_sequence_test.cpp host/pad_sequence.cpp)
target_include_directories(pad_sequence_tests PRIVATE host)
add_test(NAME pad_sequence COMMAND pad_sequence_tests)
# Every route file parses and waits for the game (no route that sleeps on vblank counts alone).
file(GLOB GT_PAD_ROUTES ${CMAKE_SOURCE_DIR}/config/pad_routes/*.pad)
add_test(NAME pad_routes COMMAND pad_sequence_tests ${GT_PAD_ROUTES})
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
