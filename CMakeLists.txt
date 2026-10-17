cmake_minimum_required(VERSION 3.20)
project(FCCVisualization VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Generated configuration header lives in the build tree, never in the sources.
set(FCCVIS_GENERATED_INCLUDE_DIR ${CMAKE_BINARY_DIR}/include)
set(FCCVIS_UNIFIED_INCLUDE_DIR ${FCCVIS_GENERATED_INCLUDE_DIR})
configure_file(${CMAKE_SOURCE_DIR}/cmake/FccvisConfig.h.in
               ${FCCVIS_GENERATED_INCLUDE_DIR}/fccvis/FccvisConfig.h @ONLY)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(FCCVIS_BUILD_SHARED   "Build shared library" ON)
option(FCCVIS_BUILD_STATIC   "Build static library" OFF)
option(FCCVIS_BUILD_TESTS    "Build unit tests"     ON)
option(FCCVIS_BUILD_EXAMPLES "Build examples"       ON)
option(FCCVIS_BUILD_DOCS "Add the docs target (Doxygen + Sphinx; needs FCCVIS_BUILD_EXAMPLES)" OFF)

if(NOT FCCVIS_BUILD_SHARED AND NOT FCCVIS_BUILD_STATIC)
  message(FATAL_ERROR "At least one of FCCVIS_BUILD_SHARED or FCCVIS_BUILD_STATIC must be ON")
endif()

include(CompilerWarnings)
include(FCCVisModules)
include(FCCVisOptions)
include(FCCVisHeaders)

# ---------------------------------------------------------------------------
# Optional third-party dependencies. Nothing here is required: the core builds
# with the standard library alone. Each adapter/backend has FCCVIS_ENABLE_<NAME>,
# defaulting to ON only when its dependency is found (see docs/DEPENDENCIES.md).
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)
find_package(ROOT CONFIG QUIET)
find_package(DD4hep CONFIG QUIET COMPONENTS DDCore)
find_package(EDM4HEP CONFIG QUIET)

set(_fccvis_have_dd4hep OFF)
if(DD4hep_FOUND AND TARGET DD4hep::DDCore AND TARGET ROOT::Geom)
  set(_fccvis_have_dd4hep ON)
endif()

set(_fccvis_have_edm4hep OFF)
if(EDM4HEP_FOUND AND TARGET EDM4HEP::edm4hep AND TARGET podio::podioIO AND TARGET podio::podioRootIO)
  set(_fccvis_have_edm4hep ON)
endif()

set(_fccvis_have_eve OFF)
if(TARGET ROOT::Eve AND TARGET ROOT::RGL AND TARGET ROOT::Gui)
  set(_fccvis_have_eve ON)
endif()

# Adapters
fccvis_dependent_option(FCCVIS_ENABLE_DD4HEP "Detector adapter A1 (DD4hep/TGeo)" ${_fccvis_have_dd4hep}
                        "DD4hep (DDCore) and ROOT (Geom) were not found")
fccvis_dependent_option(FCCVIS_ENABLE_EDM4HEP "EDM4hep adapter A2 (podio event files)" ${_fccvis_have_edm4hep}
                        "EDM4hep and podio (IO, ROOT IO) were not found")
fccvis_dependent_option(FCCVIS_ENABLE_CSV "CSV adapter A3 (no dependencies)" ON "")
fccvis_dependent_option(FCCVIS_ENABLE_PHOENIX_JSON "Phoenix event-data JSON adapter A4 (no dependencies)" ON "")
# Backends
if(DEFINED FCCVIS_ENABLE_FV)  # former name of the raster backend option
  set(FCCVIS_ENABLE_RASTER ${FCCVIS_ENABLE_FV} CACHE BOOL "" FORCE)
endif()
fccvis_dependent_option(FCCVIS_ENABLE_RASTER "B0 raster PNG backend (no dependencies)" ON "")
fccvis_dependent_option(FCCVIS_ENABLE_SVG "B2 SVG export backend (no dependencies)" ON "")
fccvis_dependent_option(FCCVIS_ENABLE_HISTOGRAM "B3 histogram maker backend (no dependencies)" ON "")
fccvis_dependent_option(FCCVIS_ENABLE_ROOT "B1 ROOT classic TEve viewer (needs X at run time)" ${_fccvis_have_eve}
                        "ROOT with Eve, RGL and Gui was not found")
set(_fccvis_have_reve OFF)
if(TARGET ROOT::ROOTEve AND NOT TARGET nlohmann_json::nlohmann_json)
  find_package(nlohmann_json CONFIG QUIET)  # ROOT::ROOTEve links it but ROOT's config does not load it
endif()
if(FCCVIS_ENABLE_ROOT AND TARGET ROOT::ROOTEve AND TARGET nlohmann_json::nlohmann_json AND TARGET ROOT::ROOTWebDisplay AND TARGET ROOT::RHTTP)
  set(_fccvis_have_reve ON)
endif()
fccvis_dependent_option(FCCVIS_ENABLE_REVE "B1 ROOT REve web viewer (part of the ROOT backend module)" ${_fccvis_have_reve}
                        "needs FCCVIS_ENABLE_ROOT, ROOT built with webgui and root7 (ROOT::ROOTEve) and nlohmann_json")
fccvis_dependent_option(FCCVIS_ENABLE_CED "B4 CED (glced) backend over its TCP protocol (POSIX sockets)" ${UNIX} "needs POSIX sockets")
fccvis_dependent_option(FCCVIS_ENABLE_PHOENIX "B5 Phoenix backend: event JSON, OBJ, HTTP host (POSIX sockets)" ${UNIX} "needs POSIX sockets")

# Python binding: needs pybind11 (found through `python3 -m pybind11 --cmakedir`)
# and the shared library (the module links it).
set(_fccvis_have_pybind OFF)
find_package(Python3 COMPONENTS Interpreter Development.Module QUIET)
if(Python3_FOUND AND FCCVIS_BUILD_SHARED)
  execute_process(COMMAND ${Python3_EXECUTABLE} -m pybind11 --cmakedir
                  OUTPUT_VARIABLE _fccvis_pybind_dir OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  find_package(pybind11 CONFIG QUIET HINTS ${_fccvis_pybind_dir})
  if(pybind11_FOUND)
    set(_fccvis_have_pybind ON)
  endif()
endif()
fccvis_dependent_option(FCCVIS_ENABLE_PYTHON "Python binding (pybind11)" ${_fccvis_have_pybind}
                        "pybind11 or the Python development files were not found, or FCCVIS_BUILD_SHARED is off")

# ---------------------------------------------------------------------------
# Sources, tests, examples
# ---------------------------------------------------------------------------
add_subdirectory(src)
fccvis_check_module_graph()
if(FCCVIS_ENABLE_PYTHON)
  add_subdirectory(src/python)
endif()

if(FCCVIS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(FCCVIS_BUILD_EXAMPLES)
  add_subdirectory(docs/examples)
endif()

if(FCCVIS_BUILD_DOCS)
  add_subdirectory(docs)
endif()

fccvis_print_options()
