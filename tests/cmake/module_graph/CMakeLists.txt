# Scratch project for the module-graph tests (configure only, nothing is built).
cmake_minimum_required(VERSION 3.20)
project(fccvis_module_graph_scratch LANGUAGES CXX)
list(APPEND CMAKE_MODULE_PATH ${FCCVIS_CMAKE_DIR})
include(CompilerWarnings)
include(FCCVisModules)

if(CASE STREQUAL "cycle")
  # Deliberate cycle between two modules: must fail before anything else.
  fccvis_add_module(geom_x KIND geometry DEPENDS data_x)
  fccvis_add_module(data_x KIND data DEPENDS geom_x)
  fccvis_add_module(filter_x KIND filter)
elseif(CASE STREQUAL "disallowed")
  # data may depend on geometry only; data -> scene breaks the architecture.
  fccvis_add_module(geom_x KIND geometry)
  fccvis_add_module(scene_x KIND scene)
  fccvis_add_module(data_x KIND data DEPENDS geom_x scene_x)
elseif(CASE STREQUAL "third_party")
  # Core modules may not link third-party targets.
  add_library(fake_root INTERFACE)
  fccvis_add_module(geom_x KIND geometry LIBS fake_root)
elseif(CASE STREQUAL "valid")
  fccvis_add_module(geom_x KIND geometry)
  fccvis_add_module(data_x KIND data DEPENDS geom_x)
  fccvis_add_module(adapter_x KIND adapter DEPENDS data_x)
else()
  message(FATAL_ERROR "unknown CASE '${CASE}'")
endif()

fccvis_check_module_graph()
