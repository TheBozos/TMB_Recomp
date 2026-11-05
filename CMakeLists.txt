cmake_minimum_required(VERSION 3.21)
project(TMB_Recomp LANGUAGES C CXX)

# The port uses no C++ modules. GCC's module mapper cannot represent canonical Windows kit paths with spaces.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# The port is built only on BozoEngine. The pre-migration build remains in the v0.3.2 tag.
include(cmake/bozo.cmake)
