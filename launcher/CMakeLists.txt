cmake_minimum_required(VERSION 3.21)
project(TMB_Launcher LANGUAGES C CXX)

# TMB Launcher.exe: what a player double-clicks. It checks their disc image, runs tools/tmb_build.py with the build kit
# (kit\python), shows its progress, and starts the game. Its own small project (the runner's CMake needs the generated
# code): raylib, Dear ImGui and rlImGui at the tags the fork's ps2xRuntime pins, built static, a GUI-subsystem exe that
# needs no DLL. Windows only.
#
#   cmake -S launcher -B build/launcher -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/launcher        -> build/launcher/TMB Launcher.exe
#
# Offline (the build kit): -DFETCHCONTENT_SOURCE_DIR_RAYLIB=<kit>/deps/raylib-src (IMGUI, RLIMGUI likewise) and
# -DLAUNCHER_FONT_DIR=<dir with Exo2[wght].ttf and OFL.txt>.

if(NOT WIN32)
    message(FATAL_ERROR "The launcher is Windows only (Linux players run tools/tmb_build.py).")
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD) # FetchContent_Populate, as the fork uses it
endif()
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib GIT_REPOSITORY "https://github.com/raysan5/raylib.git" GIT_TAG "5.5" GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG "v1.92.7-docking" GIT_SHALLOW TRUE)
FetchContent_Declare(rlImGui GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git GIT_TAG "Raylib_5_5" GIT_SHALLOW TRUE)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()
FetchContent_GetProperties(rlImGui)
if(NOT rlimgui_POPULATED)
    FetchContent_Populate(rlImGui)
endif()

add_library(launcher_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp" "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp" "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${rlimgui_SOURCE_DIR}/rlImGui.cpp")
target_include_directories(launcher_imgui PUBLIC "${imgui_SOURCE_DIR}" "${rlimgui_SOURCE_DIR}")
# No Font Awesome merged into the fonts, no demo window.
target_compile_definitions(launcher_imgui PUBLIC NO_FONT_AWESOME IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS)
target_link_libraries(launcher_imgui PUBLIC raylib)

# The type: Exo 2 (SIL Open Font License 1.1) from google/fonts at a pinned commit, embedded as a resource with its
# licence. Downloaded at configure time (never committed) unless LAUNCHER_FONT_DIR has the two files.
set(LAUNCHER_FONT_COMMIT "1796e34455b893092713d206e7f23e3e07699f00")
set(LAUNCHER_FONT_SHA256 "205a448676a2586f9c57c25f3d5c58ca8db7e6cf5edf7506783a010c6fe2bfb5")
set(LAUNCHER_FONT_DIR "" CACHE PATH "A folder with Exo2[wght].ttf and OFL.txt (google/fonts ofl/exo2) instead of the download")
set(_font_dir "${CMAKE_BINARY_DIR}/fonts")
if(LAUNCHER_FONT_DIR)
    file(COPY "${LAUNCHER_FONT_DIR}/Exo2[wght].ttf" "${LAUNCHER_FONT_DIR}/OFL.txt" DESTINATION "${_font_dir}")
elseif(NOT EXISTS "${_font_dir}/Exo2[wght].ttf")
    set(_raw "https://raw.githubusercontent.com/google/fonts/${LAUNCHER_FONT_COMMIT}/ofl/exo2")
    file(DOWNLOAD "${_raw}/Exo2%5Bwght%5D.ttf" "${_font_dir}/Exo2[wght].ttf"
        EXPECTED_HASH SHA256=${LAUNCHER_FONT_SHA256} STATUS _st)
    file(DOWNLOAD "${_raw}/OFL.txt" "${_font_dir}/OFL.txt")
endif()
file(SHA256 "${_font_dir}/Exo2[wght].ttf" _got)
if(NOT _got STREQUAL LAUNCHER_FONT_SHA256)
    message(FATAL_ERROR "Exo2[wght].ttf: SHA-256 ${_got}, expected ${LAUNCHER_FONT_SHA256} (google/fonts ${LAUNCHER_FONT_COMMIT})")
endif()
# windres does not like brackets in names.
configure_file("${_font_dir}/Exo2[wght].ttf" "${_font_dir}/exo2.ttf" COPYONLY)

# The icon, drawn by a small program at build time (launcher/tools/make_icon.cpp): no image committed.
add_executable(launcher_make_icon tools/make_icon.cpp)
set(_icon "${CMAKE_BINARY_DIR}/launcher.ico")
add_custom_command(OUTPUT "${_icon}" COMMAND launcher_make_icon "${_icon}" DEPENDS launcher_make_icon VERBATIM)
add_custom_target(launcher_icon DEPENDS "${_icon}")

set(LAUNCHER_ICON "${_icon}")
set(LAUNCHER_EXO2 "${_font_dir}/exo2.ttf")
set(LAUNCHER_OFL "${_font_dir}/OFL.txt")
configure_file(launcher.rc.in "${CMAKE_BINARY_DIR}/launcher.rc" @ONLY)
set_source_files_properties("${CMAKE_BINARY_DIR}/launcher.rc" PROPERTIES OBJECT_DEPENDS "${_icon};${LAUNCHER_EXO2}")
enable_language(RC)

add_executable(tmb_launcher WIN32
    src/main.cpp src/ui.cpp src/build_model.cpp src/config.cpp src/platform_win.cpp
    "${CMAKE_BINARY_DIR}/launcher.rc")
add_dependencies(tmb_launcher launcher_icon)
set_target_properties(tmb_launcher PROPERTIES OUTPUT_NAME "TMB Launcher")
target_link_libraries(tmb_launcher PRIVATE launcher_imgui raylib comdlg32 shell32 ole32)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # No DLL besides Windows' own: libstdc++, libgcc and winpthread linked in; no debug information.
    target_link_options(tmb_launcher PRIVATE -static -s)
endif()
