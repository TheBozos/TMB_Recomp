# TMB on BozoEngine (phase 4, BozoEngine's docs/phase4-inventory.md): TMB's runtime/ built against BozoEngine instead
# of the PS2Recomp fork. Milestone M0: headless; M1: the window (tmb), the packs built from the disc.
#
#   cmake -S . -B build/bozo -G Ninja -DBOZO_ENGINE_DIR=<BozoEngine> -DTMB_ELF=<SCUS_971.01>
#         -DTMB_ISO=<the disc image> [-DBOZO_FFMPEG_ROOT=<kit>/ffmpeg] [-DTMB_WINDOW=OFF]
#
# Targets:
# - tmb: the game in a window, in real time (SDL3, OpenGL 3.3: BozoEngine's bozo_app; TMB_WINDOW, on by default).
# - tmb_packs: the texture and mesh packs (tools/tmb_texture_pack.py, tools/tmb_mesh_pack.py on BozoEngine's
#   tools/native_pack) from the disc image's files, into <build>/packs; the runtime's built-in pack paths point there.
# - tmb-headless: the game on BozoEngine with no window (the free host: every library native, the disc image read,
#   BozoEngine's own timing); it reports the frames and the game's state.
# - tmb-check: BozoEngine's call-log checker (bozo-run, tools/run/README.md) with TMB's overrides running natively:
#   tmb-check <log.clg> --pinned --ref <run.ref> ...
#
# The generated code: bozo-recomp with the externals bozo-externals writes from the ELF (the stubs BozoEngine answers),
# recomp/tmb_stubs.txt and TMB's override list (tools/tmb_overrides_list.py, from runtime/*.cpp).

# BozoEngine: the submodule (BozoEngine/, at the commit TMB pins) unless BOZO_ENGINE_DIR names another tree (development).
set(BOZO_ENGINE_DIR "" CACHE PATH "BozoEngine's source tree (default: the BozoEngine submodule)")
if(NOT BOZO_ENGINE_DIR AND EXISTS "${CMAKE_SOURCE_DIR}/BozoEngine/CMakeLists.txt")
    set(BOZO_ENGINE_DIR "${CMAKE_SOURCE_DIR}/BozoEngine" CACHE PATH "BozoEngine's source tree (default: the BozoEngine submodule)" FORCE)
endif()
set(TMB_ELF "" CACHE FILEPATH "TMB's executable (SCUS_971.01), extracted from the disc")
set(TMB_GEN_FILES 32 CACHE STRING "How many .cpp files the generated code is split into")
set(TMB_GEN_OPT "-O1" CACHE STRING "Optimisation of the generated code")
if(WIN32)
    enable_language(RC)
    set_source_files_properties("${CMAKE_SOURCE_DIR}/runtime/app/utf8.rc" PROPERTIES
        OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/runtime/app/tmb.manifest")
endif()
function(tmb_native_manifest target)
    if(WIN32)
        target_sources(${target} PRIVATE "${CMAKE_SOURCE_DIR}/runtime/app/utf8.rc")
    endif()
endfunction()
set(TMB_ISO "" CACHE FILEPATH "TMB's disc image (the packs are built from its files)")
set(TMB_DISC_DIR "" CACHE PATH "The packs' files already extracted from the disc (tools/tmb_build.py: <dir>/.extracted marks them done); else read from TMB_ISO here")
option(TMB_WINDOW "Build tmb, the game in a window (BozoEngine's bozo_app: SDL3, OpenGL 3.3)" ON)
if(NOT BOZO_ENGINE_DIR OR NOT EXISTS "${BOZO_ENGINE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "TMB: set BOZO_ENGINE_DIR to BozoEngine's source tree")
endif()
if(NOT TMB_ELF OR NOT EXISTS "${TMB_ELF}")
    message(FATAL_ERROR "TMB: set TMB_ELF to the game's executable (SCUS_971.01)")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# BozoEngine's own TMB generation (its call-log checks) is not wanted here: TMB generates its own.
set(BOZO_TMB_ELF "" CACHE FILEPATH "" FORCE)
set(BOZO_APP ${TMB_WINDOW} CACHE BOOL "" FORCE)
add_subdirectory("${BOZO_ENGINE_DIR}" bozo)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(gen_dir "${CMAKE_BINARY_DIR}/tmb_gen")
set(native_original_dir "${CMAKE_BINARY_DIR}/native_original")
set(overrides_list "${gen_dir}/tmb_overrides.txt")
set(externals "${gen_dir}/tmb_externals.txt")
file(GLOB TMB_RUNTIME_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/runtime/*.cpp")

add_custom_command(
    OUTPUT ${overrides_list}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${gen_dir}
    COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/tmb_overrides_list.py" "${CMAKE_SOURCE_DIR}/runtime" ${overrides_list}
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/tmb_overrides_list.py" ${TMB_RUNTIME_SOURCES}
    COMMENT "Listing TMB's overrides")
add_custom_command(
    OUTPUT ${externals}
    COMMAND bozo-externals ${TMB_ELF} ${externals} --overrides ${overrides_list} --stubs "${CMAKE_SOURCE_DIR}/recomp/tmb_stubs.txt"
    DEPENDS bozo-externals ${overrides_list} "${CMAKE_SOURCE_DIR}/recomp/tmb_stubs.txt" ${TMB_ELF}
    COMMENT "Writing the externals list")

set(gen_files ${gen_dir}/tmb_functions.h ${gen_dir}/tmb_table.cpp)
math(EXPR last "${TMB_GEN_FILES} - 1")
foreach(i RANGE ${last})
    string(LENGTH "${i}" len)
    if(len EQUAL 1)
        set(n "00${i}")
    elseif(len EQUAL 2)
        set(n "0${i}")
    else()
        set(n "${i}")
    endif()
    list(APPEND gen_files ${gen_dir}/tmb_${n}.cpp)
endforeach()
add_custom_command(
    OUTPUT ${gen_files}
    COMMAND bozo-recomp ${TMB_ELF} ${gen_dir} --prefix tmb --files ${TMB_GEN_FILES} --no-comments --externals ${externals}
    DEPENDS bozo-recomp ${externals} ${TMB_ELF}
    COMMENT "Recompiling TMB")
# The generated code alone (tools/tmb_build.py's recompile step, before the compile).
add_custom_target(tmb_recompile DEPENDS ${gen_files})
add_library(tmb_generated OBJECT ${gen_files})
add_custom_command(
    OUTPUT ${native_original_dir}/tmb_hud_original.cpp ${native_original_dir}/tmb_hud_original.h
    COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/tmb_hud_fixture.py" ${gen_dir} hud ${native_original_dir}
    DEPENDS ${gen_files} "${CMAKE_SOURCE_DIR}/tools/tmb_hud_fixture.py"
    COMMENT "Extracting original HUD blocks for optional comparisons")
target_sources(tmb_generated PRIVATE ${native_original_dir}/tmb_hud_original.cpp ${native_original_dir}/tmb_hud_original.h)
add_custom_command(
    OUTPUT ${native_original_dir}/tmb_vehicle_original.cpp ${native_original_dir}/tmb_vehicle_original.h
    COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/tmb_hud_fixture.py" ${gen_dir} vehicle ${native_original_dir}
    DEPENDS ${gen_files} "${CMAKE_SOURCE_DIR}/tools/tmb_hud_fixture.py"
    COMMENT "Extracting original vehicle blocks for optional comparisons")
target_sources(tmb_generated PRIVATE ${native_original_dir}/tmb_vehicle_original.cpp ${native_original_dir}/tmb_vehicle_original.h)
target_link_libraries(tmb_generated PUBLIC bozo_runtime_headers)
target_include_directories(tmb_generated PUBLIC ${gen_dir} ${native_original_dir})
target_compile_options(tmb_generated PRIVATE ${TMB_GEN_OPT} -w)

# TMB's runtime: an object library (its port registration, BOZO_PORT, must be linked whatever references it).
add_library(tmb_runtime OBJECT ${TMB_RUNTIME_SOURCES})
target_include_directories(tmb_runtime PUBLIC "${CMAKE_SOURCE_DIR}/runtime" ${gen_dir} ${native_original_dir})
target_link_libraries(tmb_runtime PUBLIC bozo_port bozo_render bozo_libs bozo_runtime_core)
# The Cheats tab of the settings window (F2): ImGui's core and the tab list (bozo_ui, no SDL3; BOZO_SETTINGS_UI), with
# the window. The headless programs link it too and never draw it.
if(TMB_WINDOW)
    target_link_libraries(tmb_runtime PUBLIC bozo_ui)
endif()
add_dependencies(tmb_runtime tmb_generated)
set_source_files_properties("${CMAKE_SOURCE_DIR}/runtime/tmb_native_math.cpp" "${CMAKE_SOURCE_DIR}/runtime/tmb_hud_math.cpp" "${CMAKE_SOURCE_DIR}/runtime/tmb_vehicle_math.cpp" "${CMAKE_SOURCE_DIR}/runtime/tmb_hierarchy_math.cpp" PROPERTIES
    COMPILE_OPTIONS "-ffp-contract=off;-frounding-math")
target_compile_options(tmb_runtime PRIVATE -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable)
# The draw lists as the reference build (the fork) computed them (the PS2XREF1 `draws` field): it compiled TMB's runtime
# at -O2 (its RelWithDebInfo flags) and its native renderer library at -O3 (Release), both for AVX2 with FMA, and GCC
# fuses a * b + c there (-ffp-contract=fast, its default outside ISO mode) where its inlining lets it. The matrices and
# vertices TMB and the mesh submitter (BozoEngine's render/mesh_draw.cpp, moved from the fork) compute for the draws are
# those fused results, so the same flags here give the same draws (match1p: every frame; -O3 for the runtime or no FMA:
# the first difference within ten frames of the match's start). Nothing the game computes depends on them (the
# generated code and BozoEngine's libraries keep their own flags). A CPU without FMA (before 2013's Haswell, 2011's
# Bulldozer) cannot run it, as it could not run the fork's.
set(TMB_RUNTIME_ARCH "-mavx2;-mfma" CACHE STRING "Instruction set flags of TMB's runtime and the renderer (the reference build's)")
set(TMB_RUNTIME_OPT "-O2" CACHE STRING "Optimisation of TMB's runtime (the reference build's)")
target_compile_options(tmb_runtime PRIVATE ${TMB_RUNTIME_ARCH} ${TMB_RUNTIME_OPT})
target_compile_options(bozo_render PRIVATE ${TMB_RUNTIME_ARCH})
# The packs: from the disc image's files (only those they read: .TEX, .RTX, .NGP, .PTR), into the build folder; with
# TMB_DISC_DIR, from files already extracted there (tools/tmb_build.py). Both paths build the packs locally.
if(TMB_ISO OR TMB_DISC_DIR)
    set(pack_dir "${CMAKE_BINARY_DIR}/packs")
    set(disc_dir "${CMAKE_BINARY_DIR}/disc")
    set(native_pack "${BOZO_ENGINE_DIR}/tools/native_pack")
    file(GLOB native_pack_sources CONFIGURE_DEPENDS "${native_pack}/*.py")
    if(TMB_DISC_DIR)
        set(disc_dir "${TMB_DISC_DIR}")
    else()
        add_custom_command(
            OUTPUT "${disc_dir}/.extracted"
            COMMAND Python3::Interpreter "${native_pack}/ps2_disc.py" extract "${TMB_ISO}" "${disc_dir}" --ext .TEX,.RTX,.NGP,.PTR
            COMMAND ${CMAKE_COMMAND} -E touch "${disc_dir}/.extracted"
            DEPENDS "${TMB_ISO}" "${native_pack}/ps2_disc.py"
            COMMENT "Reading the packs' files from the disc image")
    endif()
    add_custom_command(
        OUTPUT "${pack_dir}/tmb_textures.pack"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${pack_dir}"
        COMMAND ${CMAKE_COMMAND} -E env "BOZO_ENGINE_TOOLS=${BOZO_ENGINE_DIR}/tools"
                "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/tmb_texture_pack.py" "${disc_dir}" "${TMB_ELF}" "${pack_dir}/tmb_textures.pack"
        DEPENDS "${disc_dir}/.extracted" "${CMAKE_SOURCE_DIR}/tools/tmb_texture_pack.py" ${native_pack_sources} "${TMB_ELF}"
        COMMENT "Building the texture pack")
    add_custom_command(
        OUTPUT "${pack_dir}/tmb_meshes.pack"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${pack_dir}"
        COMMAND ${CMAKE_COMMAND} -E env "BOZO_ENGINE_TOOLS=${BOZO_ENGINE_DIR}/tools"
                "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/tmb_mesh_pack.py" "${disc_dir}" "${pack_dir}/tmb_meshes.pack"
        DEPENDS "${disc_dir}/.extracted" "${CMAKE_SOURCE_DIR}/tools/tmb_mesh_pack.py" ${native_pack_sources}
        COMMENT "Building the mesh pack")
    add_custom_target(tmb_packs ALL DEPENDS "${pack_dir}/tmb_textures.pack" "${pack_dir}/tmb_meshes.pack")
    add_dependencies(tmb_runtime tmb_packs)
else()
    message(FATAL_ERROR "Set TMB_ISO or TMB_DISC_DIR to build the native texture and mesh packs")
endif()
target_compile_definitions(tmb_runtime PRIVATE TMB_TEXTURE_PACK="${pack_dir}/tmb_textures.pack"
                                               TMB_MESH_PACK="${pack_dir}/tmb_meshes.pack")

if(TARGET bozo_run)
    add_executable(tmb-check bozo/tmb_check.cpp)
    tmb_native_manifest(tmb-check)
    target_link_libraries(tmb-check PRIVATE tmb_runtime tmb_generated bozo_run)
    target_compile_definitions(tmb-check PRIVATE BOZO_RUN_DEFAULT_ELF="${TMB_ELF}")
    # --window: the pinned run shown in BozoEngine's window, its chosen frames captured (milestone M5's pictures).
    if(TMB_WINDOW)
        target_link_libraries(tmb-check PRIVATE bozo_app)
        target_compile_definitions(tmb-check PRIVATE TMB_CHECK_WINDOW=1)
    endif()
    if(MINGW)
        target_link_options(tmb-check PRIVATE -static -Wl,--stack,268435456)
    endif()
endif()

add_executable(tmb-headless bozo/tmb_headless.cpp)
tmb_native_manifest(tmb-headless)
target_link_libraries(tmb-headless PRIVATE tmb_runtime tmb_generated bozo_host)
target_compile_definitions(tmb-headless PRIVATE BOZO_RUN_DEFAULT_ELF="${TMB_ELF}")

# Explicit target: exact native matrix/ABI checks against the owner's generated leaf functions. No disc or game run.
add_executable(tmb-native-math-tests EXCLUDE_FROM_ALL tests/native_math_tests.cpp)
target_include_directories(tmb-native-math-tests PRIVATE "${CMAKE_SOURCE_DIR}/runtime" ${gen_dir})
target_compile_options(tmb-native-math-tests PRIVATE -ffp-contract=off -frounding-math)
target_link_libraries(tmb-native-math-tests PRIVATE tmb_runtime tmb_generated bozo_host)
if(MINGW)
    target_link_options(tmb-native-math-tests PRIVATE -static -Wl,--stack,268435456)
endif()
add_executable(tmb-native-hud-tests EXCLUDE_FROM_ALL tests/native_hud_tests.cpp)
target_include_directories(tmb-native-hud-tests PRIVATE "${CMAKE_SOURCE_DIR}/runtime" ${gen_dir})
target_compile_options(tmb-native-hud-tests PRIVATE -ffp-contract=off -frounding-math)
target_link_libraries(tmb-native-hud-tests PRIVATE tmb_runtime tmb_generated bozo_host)
if(MINGW)
    target_link_options(tmb-native-hud-tests PRIVATE -static -Wl,--stack,268435456)
endif()
add_executable(tmb-native-vehicle-tests EXCLUDE_FROM_ALL tests/native_vehicle_tests.cpp)
target_include_directories(tmb-native-vehicle-tests PRIVATE "${CMAKE_SOURCE_DIR}/runtime" ${gen_dir})
target_compile_options(tmb-native-vehicle-tests PRIVATE -ffp-contract=off -frounding-math)
target_link_libraries(tmb-native-vehicle-tests PRIVATE tmb_runtime tmb_generated bozo_host)
if(MINGW)
    target_link_options(tmb-native-vehicle-tests PRIVATE -static -Wl,--stack,268435456)
endif()
add_executable(tmb-native-hierarchy-tests EXCLUDE_FROM_ALL tests/native_hierarchy_tests.cpp)
target_include_directories(tmb-native-hierarchy-tests PRIVATE "${CMAKE_SOURCE_DIR}/runtime" ${gen_dir})
target_compile_options(tmb-native-hierarchy-tests PRIVATE -ffp-contract=off -frounding-math)
target_link_libraries(tmb-native-hierarchy-tests PRIVATE tmb_runtime tmb_generated bozo_host)
if(MINGW)
    target_link_options(tmb-native-hierarchy-tests PRIVATE -static -Wl,--stack,268435456)
endif()
if(MINGW)
    target_link_options(tmb-headless PRIVATE -static -Wl,--stack,268435456)
endif()

if(TMB_WINDOW)
    add_executable(tmb bozo/tmb_window.cpp)
    tmb_native_manifest(tmb)
    target_link_libraries(tmb PRIVATE tmb_runtime tmb_generated bozo_host bozo_app)
    target_compile_definitions(tmb PRIVATE BOZO_RUN_DEFAULT_ELF="${TMB_ELF}")
    if(WIN32)
        enable_language(RC)
        target_sources(tmb PRIVATE "${CMAKE_SOURCE_DIR}/runtime/app/tmb.rc")
        # A player's build (tools/tmb_build.py): a GUI-subsystem exe, no console window; started from a terminal it
        # still prints there (bozo::app attaches to the parent's console).
        option(TMB_GUI_SUBSYSTEM "tmb.exe without a console window (the players' build)" OFF)
        if(TMB_GUI_SUBSYSTEM)
            set_target_properties(tmb PROPERTIES WIN32_EXECUTABLE ON)
        endif()
    endif()
    if(MINGW)
        target_link_options(tmb PRIVATE -static -Wl,--stack,268435456)
    endif()
endif()
