# FCCVisModules.cmake
#
# Module declaration and enforcement of the module graph from ARCHITECTURE.md.
#
#   fccvis_add_module(<name> KIND <kind>
#                     [SOURCES <src>...] [INCLUDE_DIR <dir>]
#                     [DEPENDS <module>...] [LIBS <third-party target>...]
#                     [NOT_IN_LIBRARY])
#
# Creates the OBJECT library `fccvis_<name>`. Every header in INCLUDE_DIR is
# compiled once in a generated translation unit, so each public header must be
# self-contained and warning-free. DEPENDS names other fccvis modules (without
# the prefix); LIBS names third-party targets and is only allowed for adapter
# and backend implementations (no ROOT/podio/DD4hep in core public headers).
#
# fccvis_check_module_graph() must run after all modules are declared. It fails
# the configure step when a dependency is unknown, not allowed by the graph
# below, or part of a cycle. It also writes fccvis_module_graph.json, which the
# include checker test reads.

# ---------------------------------------------------------------------------
# Allowed direct dependencies per module kind (ARCHITECTURE.md "Module graph").
# A module may depend on any module whose kind is in the transitive closure.
# ---------------------------------------------------------------------------
set(FCCVIS_ALLOWED_geometry     "")
set(FCCVIS_ALLOWED_filter       "")
set(FCCVIS_ALLOWED_merge        "")
set(FCCVIS_ALLOWED_data         "geometry")
set(FCCVIS_ALLOWED_style        "filter;merge;data")
set(FCCVIS_ALLOWED_scene        "data;filter;style;merge")
set(FCCVIS_ALLOWED_changes      "scene;data")
set(FCCVIS_ALLOWED_backend      "changes;scene;data")
set(FCCVIS_ALLOWED_session      "backend;changes;scene;data")
set(FCCVIS_ALLOWED_stdlib       "geometry;filter;style;data")
set(FCCVIS_ALLOWED_adapters     "data")
set(FCCVIS_ALLOWED_adapter      "adapters;data")
set(FCCVIS_ALLOWED_backend_impl "backend;stdlib")
set(FCCVIS_ALLOWED_api          "session;stdlib;adapters")
set(FCCVIS_ALLOWED_cli          "api;session;stdlib;adapters;backend")
set(FCCVIS_THIRD_PARTY_KINDS    "adapter;backend_impl;cli")

set(_FCCVIS_MODULES_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR})

function(_fccvis_kind_closure KIND OUT)
  set(result "")
  set(queue ${FCCVIS_ALLOWED_${KIND}})
  while(queue)
    list(POP_FRONT queue k)
    if(NOT k IN_LIST result)
      list(APPEND result ${k})
      list(APPEND queue ${FCCVIS_ALLOWED_${k}})
    endif()
  endwhile()
  set(${OUT} "${result}" PARENT_SCOPE)
endfunction()

function(fccvis_add_module NAME)
  cmake_parse_arguments(M "NOT_IN_LIBRARY" "KIND;INCLUDE_DIR" "SOURCES;DEPENDS;LIBS" ${ARGN})
  if(NOT M_KIND)
    set(M_KIND ${NAME})
  endif()
  if(NOT DEFINED FCCVIS_ALLOWED_${M_KIND})
    message(FATAL_ERROR "fccvis module '${NAME}': unknown kind '${M_KIND}'")
  endif()
  if(M_LIBS AND NOT M_KIND IN_LIST FCCVIS_THIRD_PARTY_KINDS)
    message(FATAL_ERROR
      "fccvis module '${NAME}' (kind ${M_KIND}) may not link third-party targets (${M_LIBS}); "
      "only adapters and backend implementations may.")
  endif()
  if(NOT M_INCLUDE_DIR AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set(M_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
  endif()

  # Generated translation unit: every public header compiles on its own.
  set(tu ${CMAKE_CURRENT_BINARY_DIR}/fccvis_${NAME}_headers.cc)
  set(content "// Generated: compiles every public header of module '${NAME}'.\n")
  if(M_INCLUDE_DIR)
    file(GLOB_RECURSE headers CONFIGURE_DEPENDS ${M_INCLUDE_DIR}/*.h)
    list(SORT headers)
    foreach(h ${headers})
      file(RELATIVE_PATH hn ${M_INCLUDE_DIR} ${h})
      string(APPEND content "#include \"${hn}\"\n")
    endforeach()
  endif()
  file(CONFIGURE OUTPUT ${tu} CONTENT "${content}")

  set(target fccvis_${NAME})
  add_library(${target} OBJECT ${M_SOURCES} ${tu})
  if(M_INCLUDE_DIR)
    target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${M_INCLUDE_DIR}>)
  endif()
  foreach(dep ${M_DEPENDS})
    target_link_libraries(${target} PUBLIC fccvis_${dep})
  endforeach()
  if(M_LIBS)
    target_link_libraries(${target} PUBLIC ${M_LIBS})
  endif()
  target_link_libraries(${target} PRIVATE FCCVisualization::CompilerWarnings)
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)

  set_property(GLOBAL APPEND PROPERTY FCCVIS_MODULES ${NAME})
  set_property(GLOBAL PROPERTY FCCVIS_MODULE_${NAME}_KIND ${M_KIND})
  set_property(GLOBAL PROPERTY FCCVIS_MODULE_${NAME}_DEPENDS "${M_DEPENDS}")
  set_property(GLOBAL PROPERTY FCCVIS_MODULE_${NAME}_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  set_property(GLOBAL PROPERTY FCCVIS_MODULE_${NAME}_LIBS "${M_LIBS}")
  if(NOT M_NOT_IN_LIBRARY)
    set_property(GLOBAL APPEND PROPERTY FCCVIS_LIBRARY_MODULES ${NAME})
  endif()
endfunction()

function(_fccvis_visit NAME PATH)
  get_property(state GLOBAL PROPERTY _FCCVIS_VISIT_${NAME})
  if(state STREQUAL "done")
    return()
  endif()
  if(state STREQUAL "active")
    list(APPEND PATH ${NAME})
    list(JOIN PATH " -> " chain)
    message(FATAL_ERROR "fccvis module dependency cycle: ${chain}")
  endif()
  set_property(GLOBAL PROPERTY _FCCVIS_VISIT_${NAME} active)
  get_property(deps GLOBAL PROPERTY FCCVIS_MODULE_${NAME}_DEPENDS)
  set(next ${PATH})
  list(APPEND next ${NAME})
  foreach(dep ${deps})
    _fccvis_visit(${dep} "${next}")
  endforeach()
  set_property(GLOBAL PROPERTY _FCCVIS_VISIT_${NAME} done)
endfunction()

function(fccvis_check_module_graph)
  get_property(modules GLOBAL PROPERTY FCCVIS_MODULES)
  # 1. every dependency names a declared module
  foreach(m ${modules})
    get_property(deps GLOBAL PROPERTY FCCVIS_MODULE_${m}_DEPENDS)
    foreach(dep ${deps})
      if(NOT dep IN_LIST modules)
        message(FATAL_ERROR "fccvis module '${m}' depends on unknown module '${dep}'")
      endif()
    endforeach()
  endforeach()
  # 2. no cycles
  foreach(m ${modules})
    _fccvis_visit(${m} "")
  endforeach()
  # 3. every edge is allowed by the architecture graph
  set(json "{\n  \"modules\": {\n")
  set(first TRUE)
  foreach(m ${modules})
    get_property(kind GLOBAL PROPERTY FCCVIS_MODULE_${m}_KIND)
    get_property(deps GLOBAL PROPERTY FCCVIS_MODULE_${m}_DEPENDS)
    get_property(dir GLOBAL PROPERTY FCCVIS_MODULE_${m}_DIR)
    _fccvis_kind_closure(${kind} allowed)
    foreach(dep ${deps})
      get_property(depkind GLOBAL PROPERTY FCCVIS_MODULE_${dep}_KIND)
      if(NOT depkind IN_LIST allowed)
        message(FATAL_ERROR
          "fccvis module '${m}' (kind ${kind}) may not depend on '${dep}' (kind ${depkind}); "
          "allowed kinds: ${allowed}")
      endif()
    endforeach()
    # JSON entry for the include checker
    set(dq "")
    foreach(dep ${deps})
      list(APPEND dq "\"${dep}\"")
    endforeach()
    list(JOIN dq ", " dqs)
    set(aq "")
    foreach(a ${allowed})
      list(APPEND aq "\"${a}\"")
    endforeach()
    list(JOIN aq ", " aqs)
    if(NOT first)
      string(APPEND json ",\n")
    endif()
    set(first FALSE)
    string(APPEND json "    \"${m}\": {\"kind\": \"${kind}\", \"dir\": \"${dir}\", \"depends\": [${dqs}], \"allowed_kinds\": [${aqs}]}")
  endforeach()
  string(APPEND json "\n  }\n}\n")
  file(WRITE ${CMAKE_BINARY_DIR}/fccvis_module_graph.json "${json}")
  list(LENGTH modules n)
  message(STATUS "fccvis: module graph checked (${n} modules, acyclic, edges allowed)")
endfunction()
