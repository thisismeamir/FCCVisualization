# ---------------------------------------------------------------------------
# Core modules (standard library only). Order follows the module graph in
# ARCHITECTURE.md; fccvis_check_module_graph() verifies it after this file.
# ---------------------------------------------------------------------------
add_subdirectory(geometry)
add_subdirectory(filter)
add_subdirectory(merge)
add_subdirectory(data)
add_subdirectory(style)
add_subdirectory(scene)
add_subdirectory(changes)
add_subdirectory(backend)
add_subdirectory(session)
add_subdirectory(stdlib)

# Adapters (each behind FCCVIS_ENABLE_<NAME>) and the API layer
add_subdirectory(adapters)
add_subdirectory(api)

# ---------------------------------------------------------------------------
# Library: all declared modules' objects. Registrars live in these objects, so
# linking the shared library registers every enabled adapter and backend.
# ---------------------------------------------------------------------------
# Linking an OBJECT library directly adds its objects; consumers of the
# library only inherit usage requirements, so objects are linked exactly once.
get_property(_fccvis_lib_modules GLOBAL PROPERTY FCCVIS_LIBRARY_MODULES)
set(_fccvis_objects ${CMAKE_CURRENT_BINARY_DIR}/fccvis_library.cc)
file(CONFIGURE OUTPUT ${_fccvis_objects}
     CONTENT "// Generated: the library is assembled from the fccvis_* OBJECT modules.\n")

function(_fccvis_finish_library tgt)
  foreach(_m ${_fccvis_lib_modules})
    target_link_libraries(${tgt} PUBLIC fccvis_${_m})
  endforeach()
  target_include_directories(${tgt} PUBLIC $<BUILD_INTERFACE:${FCCVIS_GENERATED_INCLUDE_DIR}>)
  target_link_libraries(${tgt} PUBLIC Threads::Threads)
  # Registrars run only if the library is loaded; consumers may reference no
  # symbol of it directly (they reach backends through the registry), so the
  # linker must not drop it under --as-needed.
  if(UNIX AND NOT APPLE)
    target_link_options(${tgt} INTERFACE "LINKER:--no-as-needed")
  endif()
  set_target_properties(${tgt} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endfunction()

if(FCCVIS_BUILD_SHARED)
  add_library(FCCVisualization SHARED ${_fccvis_objects})
  add_library(FCCVisualization::FCCVisualization ALIAS FCCVisualization)
  set_target_properties(FCCVisualization PROPERTIES
    VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
  _fccvis_finish_library(FCCVisualization)
endif()

if(FCCVIS_BUILD_STATIC)
  add_library(FCCVisualization_static STATIC ${_fccvis_objects})
  add_library(FCCVisualization::FCCVisualization_static ALIAS FCCVisualization_static)
  set_target_properties(FCCVisualization_static PROPERTIES OUTPUT_NAME FCCVisualization)
  _fccvis_finish_library(FCCVisualization_static)
endif()

if(TARGET FCCVisualization::FCCVisualization)
  set(FCCVIS_LINK_LIB FCCVisualization::FCCVisualization CACHE INTERNAL "")
else()
  # Static archives drop unreferenced registrar objects; link whole-archive.
  set(FCCVIS_LINK_LIB "$<LINK_LIBRARY:WHOLE_ARCHIVE,FCCVisualization::FCCVisualization_static>" CACHE INTERNAL "")
endif()

add_subdirectory(cli)

# Header symlinks under <build>/include/fccvis for out-of-tree users.
foreach(_m ${_fccvis_lib_modules})
  get_property(_d GLOBAL PROPERTY FCCVIS_MODULE_${_m}_DIR)
  fccvis_export_headers_from(${_d}/include)
endforeach()
