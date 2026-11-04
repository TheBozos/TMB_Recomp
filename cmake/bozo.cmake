# TMB on BozoEngine (phase 4, BozoEngine's docs/phase4-inventory.md): TMB's runtime/ built against BozoEngine instead
# of the PS2Recomp fork. Milestone M0: headless.
#
#   cmake -S . -B build/bozo -G Ninja -DTMB_BOZO=ON -DBOZO_ENGINE_DIR=<BozoEngine> -DTMB_ELF=<SCUS_971.01>
#         [-DBOZO_FFMPEG_ROOT=<kit>/ffmpeg]
#
# Targets:
# - tmb-headless: the game on BozoEngine with no window (the free host: every library native, the disc image read,
#   BozoEngine's own timing); it reports the frames and the game's state.
# - tmb-check: BozoEngine's call-log checker (bozo-run, tools/run/README.md) with TMB's overrides running natively:
#   tmb-check <log.clg> --pinned --ref <run.ref> ...
#
# The generated code: bozo-recomp with the externals bozo-externals writes from the ELF (the stubs BozoEngine answers),
# recomp/tmb_stubs.txt and TMB's override list (tools/tmb_overrides_list.py, from runtime/*.cpp).

set(BOZO_ENGINE_DIR "" CACHE PATH "BozoEngine's source tree")
set(TMB_ELF "" CACHE FILEPATH "TMB's executable (SCUS_971.01), extracted from the disc")
set(TMB_GEN_FILES 32 CACHE STRING "How many .cpp files the generated code is split into")
set(TMB_GEN_OPT "-O1" CACHE STRING "Optimisation of the generated code")
if(NOT BOZO_ENGINE_DIR OR NOT EXISTS "${BOZO_ENGINE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "TMB_BOZO: set BOZO_ENGINE_DIR to BozoEngine's source tree")
endif()
if(NOT TMB_ELF OR NOT EXISTS "${TMB_ELF}")
    message(FATAL_ERROR "TMB_BOZO: set TMB_ELF to the game's executable (SCUS_971.01)")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# BozoEngine's own TMB generation (its call-log checks) is not wanted here: TMB generates its own.
set(BOZO_TMB_ELF "" CACHE FILEPATH "" FORCE)
add_subdirectory("${BOZO_ENGINE_DIR}" bozo)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(gen_dir "${CMAKE_BINARY_DIR}/tmb_gen")
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
add_library(tmb_generated OBJECT ${gen_files})
target_link_libraries(tmb_generated PUBLIC bozo_runtime_headers)
target_include_directories(tmb_generated PUBLIC ${gen_dir})
target_compile_options(tmb_generated PRIVATE ${TMB_GEN_OPT} -w)

# TMB's runtime: an object library (its port registration, BOZO_PORT, must be linked whatever references it).
add_library(tmb_runtime OBJECT ${TMB_RUNTIME_SOURCES})
target_include_directories(tmb_runtime PUBLIC "${CMAKE_SOURCE_DIR}/runtime" ${gen_dir})
target_link_libraries(tmb_runtime PUBLIC bozo_port bozo_render bozo_libs bozo_runtime_core)
add_dependencies(tmb_runtime tmb_generated)
target_compile_options(tmb_runtime PRIVATE -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable)
target_compile_definitions(tmb_runtime PRIVATE TMB_TEXTURE_PACK="${CMAKE_SOURCE_DIR}/generated_us/tmb_textures.pack"
                                               TMB_MESH_PACK="${CMAKE_SOURCE_DIR}/generated_us/tmb_meshes.pack")

if(TARGET bozo_run)
    add_executable(tmb-check bozo/tmb_check.cpp)
    target_link_libraries(tmb-check PRIVATE tmb_runtime tmb_generated bozo_run)
    target_compile_definitions(tmb-check PRIVATE BOZO_RUN_DEFAULT_ELF="${TMB_ELF}")
    if(MINGW)
        target_link_options(tmb-check PRIVATE -static -Wl,--stack,268435456)
    endif()
endif()

add_executable(tmb-headless bozo/tmb_headless.cpp)
target_link_libraries(tmb-headless PRIVATE tmb_runtime tmb_generated bozo_host)
target_compile_definitions(tmb-headless PRIVATE BOZO_RUN_DEFAULT_ELF="${TMB_ELF}")
if(MINGW)
    target_link_options(tmb-headless PRIVATE -static -Wl,--stack,268435456)
endif()
