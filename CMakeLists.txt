cmake_minimum_required(VERSION 3.20)
project(FCCVisualization VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(FCCVIS_BUILD_SHARED   "Build shared library"        ON)
option(FCCVIS_BUILD_STATIC   "Build static library"        OFF)
option(FCCVIS_ENABLE_GUI     "Enable ROOT/TEve GUI backend" OFF)
option(FCCVIS_BUILD_TESTS    "Build unit tests"             ON)
option(FCCVIS_BUILD_EXAMPLES "Build examples"               ON)

if(NOT FCCVIS_BUILD_SHARED AND NOT FCCVIS_BUILD_STATIC)
    message(FATAL_ERROR "At least one of FCCVIS_BUILD_SHARED or FCCVIS_BUILD_STATIC must be ON")
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
if(NOT TARGET PandoraPFA::PandoraSDK)
    find_package(PandoraSDK REQUIRED)
endif()

find_package(ROOT REQUIRED COMPONENTS Core RIO Eve Geom RGL EG)

if(FCCVIS_ENABLE_GUI)
    find_package(ROOT REQUIRED COMPONENTS Gui)
endif()
# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
include(cmake/CompilerWarnings.cmake)
add_subdirectory(src)

if(FCCVIS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(FCCVIS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
