cmake_minimum_required(VERSION 3.20)
project(FCCVisualization VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(FCCVIS_UNIFIED_INCLUDE_DIR ${CMAKE_SOURCE_DIR})
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/FccvisConfig.h.in
    ${FCCVIS_UNIFIED_INCLUDE_DIR}/fccvis/FccvisConfig.h
    @ONLY
)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(FCCVIS_BUILD_SHARED   "Build shared library"        ON)
option(FCCVIS_BUILD_STATIC   "Build static library"        OFF)
option(FCCVIS_ENABLE_GUI     "Enable ROOT/TEve GUI backend" ON)
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

list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_SOURCE_DIR}/cmake"
)

include(FCCVisHeaders)

fccvis_export_headers(filter)
fccvis_export_headers(api)
fccvis_export_headers(core)
fccvis_export_headers(backend)
fccvis_export_headers(visualization)
fccvis_export_headers(session)
fccvis_export_headers(gui)
fccvis_export_headers(scene)
fccvis_export_headers(cli)
fccvis_export_headers(geometry)
fccvis_export_headers(data)
