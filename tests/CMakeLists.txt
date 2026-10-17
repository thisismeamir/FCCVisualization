find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
  message(WARNING "Catch2 v3 not found: unit tests are not built (FCCVIS_BUILD_TESTS ignored)")
  return()
endif()
include(CTest)
include(Catch)
find_package(Threads REQUIRED)

# fccvis_add_test(<name> SOURCES <src>... [LIBS <extra>...] [LABELS <label>...]
#                 [ENVIRONMENT <var=value>...] [ENVIRONMENT_MODIFICATION <var=op:value>...])
# One executable per test group, linked against the full library (so every
# enabled adapter/backend registrar is present), discovered by CTest.
# LABELS defaults to "core"; see docs/DEPENDENCIES.md for the label set.
function(fccvis_add_test NAME)
  cmake_parse_arguments(T "" "" "SOURCES;LIBS;LABELS;ENVIRONMENT;ENVIRONMENT_MODIFICATION" ${ARGN})
  if(NOT T_LABELS)
    set(T_LABELS core)
  endif()
  add_executable(${NAME} ${T_SOURCES})
  target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
  target_link_libraries(${NAME} PRIVATE ${FCCVIS_LINK_LIB} ${T_LIBS} Catch2::Catch2WithMain Threads::Threads)
  target_compile_definitions(${NAME} PRIVATE
    FCCVIS_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    FCCVIS_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
  set_target_properties(${NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  # Catch2 exits with 4 when a test calls SKIP (missing optional data, display, ...).
  catch_discover_tests(${NAME} TEST_PREFIX "${NAME}::" PROPERTIES SKIP_RETURN_CODE 4)
  # Labels and environment are set from a CTest include file that runs after Catch's:
  # catch_discover_tests flattens list-valued properties, so only the first item would survive.
  # (Test names must therefore not contain ";"; build::labels reports such tests as unlabelled.)
  set(_labels_file "${CMAKE_CURRENT_BINARY_DIR}/${NAME}_labels.cmake")
  set(_env "")
  if(T_ENVIRONMENT)
    set(_env " ENVIRONMENT \"${T_ENVIRONMENT}\"")
  endif()
  if(T_ENVIRONMENT_MODIFICATION)
    string(APPEND _env " ENVIRONMENT_MODIFICATION \"${T_ENVIRONMENT_MODIFICATION}\"")
  endif()
  file(WRITE "${_labels_file}"
       "foreach(_t IN LISTS ${NAME}_TESTS)\n  set_tests_properties(\"\${_t}\" PROPERTIES LABELS \"${T_LABELS}\"${_env})\nendforeach()\n")
  set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${_labels_file}")
endfunction()

# ---------------------------------------------------------------------------
# Core
# ---------------------------------------------------------------------------
fccvis_add_test(fccvis_geometry_tests SOURCES
  geometry/test_abstracts.cc
  geometry/test_shapes.cc
  geometry/test_transformations.cc
  geometry/test_placement.cc
  geometry/test_adapters_basics.cc)

set(_backend_sources backend/fccvis_recordingbackend_tests.cc)
if(FCCVIS_ENABLE_RASTER)
  list(APPEND _backend_sources backend/fccvis_fv_tests.cc backend/test_raster.cc)
endif()
if(FCCVIS_ENABLE_SVG)
  list(APPEND _backend_sources backend/test_svg.cc)
endif()
if(FCCVIS_ENABLE_HISTOGRAM AND FCCVIS_ENABLE_SVG)
  list(APPEND _backend_sources backend/test_histogram.cc)
endif()
if(FCCVIS_ENABLE_PHOENIX)
  list(APPEND _backend_sources backend/test_phoenix.cc)
endif()
fccvis_add_test(fccvis_backend_tests SOURCES ${_backend_sources})

fccvis_add_test(fccvis_session_tests SOURCES
  session/test_capabilities.cc
  session/test_remove_scene.cc
  session/test_camera_fixed.cc
  session/test_series_append.cc
  session/test_undo.cc)

fccvis_add_test(fccvis_stdlib_tests SOURCES
  stdlib/test_shapes.cc
  stdlib/test_filters.cc
  stdlib/test_shape_filters.cc
  stdlib/test_detector_filters.cc
  stdlib/test_styles.cc)

fccvis_add_test(fccvis_api_tests SOURCES
  api/test_builders.cc
  api/test_expression.cc)
if(FCCVIS_ENABLE_CSV AND FCCVIS_ENABLE_RASTER AND FCCVIS_ENABLE_SVG AND FCCVIS_ENABLE_HISTOGRAM)
  fccvis_add_test(fccvis_completion_tests SOURCES api/test_completion.cc api/test_help.cc)
endif()
if(FCCVIS_ENABLE_CSV AND FCCVIS_ENABLE_RASTER)
  fccvis_add_test(fccvis_shell_tests SOURCES api/test_interpreter.cc api/test_config.cc)
endif()

# ---------------------------------------------------------------------------
# Backend contract: one CTest entry per enabled backend (plus the recording
# reference). FCCVIS_CONTRACT_BACKENDS lists "type:label;label" pairs.
# ---------------------------------------------------------------------------
add_executable(fccvis_backend_contract_tests backend/backend_contract_tests.cc)
target_include_directories(fccvis_backend_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(fccvis_backend_contract_tests PRIVATE ${FCCVIS_LINK_LIB} Catch2::Catch2WithMain Threads::Threads)
set_target_properties(fccvis_backend_contract_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_executable(fccvis_backend_matrix_tests backend/backend_matrix_tests.cc)
target_include_directories(fccvis_backend_matrix_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(fccvis_backend_matrix_tests PRIVATE ${FCCVIS_LINK_LIB} Catch2::Catch2WithMain Threads::Threads)
set_target_properties(fccvis_backend_matrix_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
set(_contract recording:core)
if(FCCVIS_ENABLE_RASTER)
  list(APPEND _contract raster:core fv:core)
endif()
if(FCCVIS_ENABLE_SVG)
  list(APPEND _contract svg:core)
endif()
if(FCCVIS_ENABLE_HISTOGRAM)
  list(APPEND _contract histogram:core)
endif()
if(FCCVIS_ENABLE_PHOENIX)
  list(APPEND _contract phoenix:core)
endif()
if(FCCVIS_ENABLE_CED)
  list(APPEND _contract ced:core)
endif()
# Backends that need an X display run under xvfb-run when it is installed
# (FCCVIS_X11_LAUNCHER, empty to use the current DISPLAY).
find_program(FCCVIS_XVFB_RUN xvfb-run)
set(_x11_default "")
if(FCCVIS_XVFB_RUN)
  set(_x11_default "${FCCVIS_XVFB_RUN};-a;-s;-screen 0 1280x1024x24")
endif()
set(FCCVIS_X11_LAUNCHER "${_x11_default}" CACHE STRING "Command prefix for tests that need an X display")
if(FCCVIS_ENABLE_ROOT)
  list(APPEND _contract root:needs-root,needs-x11)
endif()
if(FCCVIS_ENABLE_REVE)
  list(APPEND _contract reve:needs-root)
endif()
foreach(_c ${_contract} ${FCCVIS_CONTRACT_BACKENDS})
  string(REPLACE ":" ";" _parts ${_c})
  list(GET _parts 0 _type)
  list(GET _parts 1 _labels)
  string(REPLACE "," ";" _labels "${_labels}")
  set(_launcher "")
  if("needs-x11" IN_LIST _labels)
    set(_launcher ${FCCVIS_X11_LAUNCHER})
  endif()
  add_test(NAME contract::${_type} COMMAND ${_launcher} $<TARGET_FILE:fccvis_backend_contract_tests> "[contract]")
  add_test(NAME matrix::${_type} COMMAND ${_launcher} $<TARGET_FILE:fccvis_backend_matrix_tests> "[matrix]")
  set_tests_properties(matrix::${_type} PROPERTIES
    ENVIRONMENT "FCCVIS_MATRIX_BACKEND=${_type}" LABELS "${_labels}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(contract::${_type} PROPERTIES
    ENVIRONMENT "FCCVIS_CONTRACT_BACKEND=${_type}" LABELS "${_labels}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()

# CED backend: protocol tests against a mock server; with glced (FCCVIS_GLCED,
# found on PATH or in $CED_DIR/bin) and xdotool also against the real server.
if(FCCVIS_ENABLE_CED)
  fccvis_add_test(fccvis_ced_tests SOURCES backend/test_ced.cc)
  target_include_directories(fccvis_ced_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/backend/impl/ced)
  find_program(FCCVIS_GLCED glced HINTS $ENV{CED_DIR}/bin)
  find_program(FCCVIS_XDOTOOL xdotool)
  if(FCCVIS_GLCED AND FCCVIS_XDOTOOL)
    add_test(NAME ced::glced COMMAND ${CMAKE_COMMAND} -E env FCCVIS_GLCED=${FCCVIS_GLCED}
                                     ${FCCVIS_X11_LAUNCHER} $<TARGET_FILE:fccvis_ced_tests> "[ced-glced]")
    set_tests_properties(ced::glced PROPERTIES LABELS "needs-ced;needs-x11" SKIP_RETURN_CODE 4 TIMEOUT 120)
  endif()
endif()

# ROOT TEve backend: run under the X launcher; [root-close] closes the Eve
# window for good, so it gets its own process.
if(FCCVIS_ENABLE_ROOT)
  add_executable(fccvis_root_tests backend/test_root.cc)
  target_include_directories(fccvis_root_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support
                             ${PROJECT_SOURCE_DIR}/src/backend/impl/root)
  target_link_libraries(fccvis_root_tests PRIVATE ${FCCVIS_LINK_LIB} ROOT::Eve ROOT::RGL ROOT::Gui
                        Catch2::Catch2WithMain Threads::Threads)
  target_compile_definitions(fccvis_root_tests PRIVATE FCCVIS_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
  set_target_properties(fccvis_root_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME root::viewer COMMAND ${FCCVIS_X11_LAUNCHER} $<TARGET_FILE:fccvis_root_tests> "[root]")
  add_test(NAME root::close COMMAND ${FCCVIS_X11_LAUNCHER} $<TARGET_FILE:fccvis_root_tests> "[root-close]")
  add_test(NAME root::nodisplay COMMAND $<TARGET_FILE:fccvis_root_tests> "[root-nodisplay]")
  set_tests_properties(root::viewer root::close PROPERTIES LABELS "needs-root;needs-x11" SKIP_RETURN_CODE 4)
  set_tests_properties(root::nodisplay PROPERTIES LABELS "needs-root")
endif()
# REve web backend: needs Node.js with Playwright (installed by tools/phoenix_verify).
find_program(FCCVIS_NODE node)
set(_node_modules ${PROJECT_SOURCE_DIR}/tools/phoenix_verify/node_modules)
if(FCCVIS_ENABLE_REVE)
  set(_reve_node "")
  if(FCCVIS_NODE AND EXISTS ${_node_modules}/playwright)
    set(_reve_node ${FCCVIS_NODE})
  endif()
  fccvis_add_test(fccvis_reve_tests SOURCES backend/test_reve.cc LABELS needs-root needs-node
    ENVIRONMENT "FCCVIS_NODE=${_reve_node};FCCVIS_NODE_MODULES=${_node_modules}")
endif()

# ---------------------------------------------------------------------------
# Adapters
# ---------------------------------------------------------------------------
if(FCCVIS_ENABLE_CSV)
  fccvis_add_test(fccvis_adapter_csv_tests SOURCES adapters/csv/test_csv.cc)
endif()
if(FCCVIS_ENABLE_DD4HEP)
  # DD4hep finds its detector drivers through LD_LIBRARY_PATH (.components files).
  get_target_property(_ddcore DD4hep::DDCore LOCATION)
  get_filename_component(_ddlib "${_ddcore}" DIRECTORY)
  get_target_property(_rootcore ROOT::Core LOCATION)
  get_filename_component(_rootlib "${_rootcore}" DIRECTORY)
  # Prepended at test time, so drivers on the caller's LD_LIBRARY_PATH (for
  # example k4geo's, for the CLD tests) are found as well.
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
    set(_ddenv ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:${_ddlib}:${_rootlib}")
  else()
    set(_ddenv ENVIRONMENT "LD_LIBRARY_PATH=${_ddlib}:${_rootlib}:$ENV{LD_LIBRARY_PATH}")
  endif()
  fccvis_add_test(fccvis_adapter_detector_tests SOURCES
    adapters/detector/test_adapters_tgeo.cc
    adapters/detector/test_detector_node.cc
    adapters/detector/test_detector_adapter.cc
    LABELS needs-root needs-dd4hep ${_ddenv})
  # Need a real compact file in FCCVIS_TEST_COMPACT; skip themselves otherwise.
  fccvis_add_test(fccvis_adapter_detector_data_tests SOURCES
    adapters/detector/test_detector_tree.cc
    adapters/detector/test_adapters_pipeline.cc
    LABELS needs-root needs-dd4hep needs-data ${_ddenv})
endif()

if(FCCVIS_ENABLE_EDM4HEP)
  # podio finds the EDM4hep ROOT dictionaries through LD_LIBRARY_PATH (.rootmap files).
  set(_edmlibs "")
  foreach(_t EDM4HEP::edm4hep podio::podio ROOT::Core)
    get_target_property(_loc ${_t} LOCATION)
    get_filename_component(_dir "${_loc}" DIRECTORY)
    list(APPEND _edmlibs ${_dir})
  endforeach()
  list(REMOVE_DUPLICATES _edmlibs)
  string(REPLACE ";" ":" _edmlibs "${_edmlibs}")
  fccvis_add_test(fccvis_adapter_edm4hep_tests SOURCES adapters/edm4hep/test_edm4hep.cc
    LABELS needs-root needs-podio needs-edm4hep ENVIRONMENT "LD_LIBRARY_PATH=${_edmlibs}:$ENV{LD_LIBRARY_PATH}")
  # Regenerates the committed fixture (not run by the tests; see the source).
  add_executable(fccvis_edm4hep_make_fixture data/edm4hep/make_fixture.cc)
  target_link_libraries(fccvis_edm4hep_make_fixture PRIVATE EDM4HEP::edm4hep podio::podioIO podio::podioRootIO)
  set_target_properties(fccvis_edm4hep_make_fixture PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endif()

if(FCCVIS_ENABLE_PHOENIX_JSON)
  set(_phoenix_json_sources adapters/phoenix/test_phoenix_adapter.cc)
  fccvis_add_test(fccvis_adapter_phoenix_tests SOURCES ${_phoenix_json_sources})
  fccvis_add_test(fccvis_adapter_phoenix_data_tests SOURCES adapters/phoenix/test_phoenix_adapter_data.cc
    LABELS needs-data)
endif()

# ---------------------------------------------------------------------------
# End-to-end
# ---------------------------------------------------------------------------
if(FCCVIS_ENABLE_CSV AND FCCVIS_ENABLE_RASTER)
  fccvis_add_test(fccvis_e2e_tests SOURCES e2e/test_csv_to_png.cc)
endif()

add_subdirectory(shell)

# The committed Phoenix golden (equal to what the backend writes, see
# test_phoenix.cc) follows the documented Phoenix event format.
if(FCCVIS_ENABLE_PHOENIX)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    add_test(NAME phoenix::format
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/backend/phoenix_check.py
                     ${CMAKE_CURRENT_SOURCE_DIR}/data/golden/phoenix_events.json)
    set_tests_properties(phoenix::format PROPERTIES LABELS core)
  endif()
  # The output loaded into the Phoenix event display (tools/phoenix_verify): only when
  # node and the tool's npm packages are installed.
  find_program(FCCVIS_NODE node)
  if(FCCVIS_NODE AND FCCVIS_ENABLE_CSV AND EXISTS ${PROJECT_SOURCE_DIR}/tools/phoenix_verify/node_modules/phoenix-event-display)
    add_test(NAME phoenix::viewer
             COMMAND ${CMAKE_COMMAND} -DSHELL=$<TARGET_FILE:fccvis-shell>
                     -DSCRIPT=${PROJECT_SOURCE_DIR}/docs/examples/phoenix_session.fcv
                     -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/phoenix-viewer -DNODE=${FCCVIS_NODE}
                     -DVERIFY=${PROJECT_SOURCE_DIR}/tools/phoenix_verify/verify.mjs
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/backend/RunPhoenixVerify.cmake)
    set_tests_properties(phoenix::viewer PROPERTIES LABELS needs-node TIMEOUT 180)
  endif()
endif()

# ---------------------------------------------------------------------------
# Python binding
# ---------------------------------------------------------------------------
if(FCCVIS_ENABLE_PYTHON AND FCCVIS_ENABLE_CSV AND FCCVIS_ENABLE_RASTER)
  add_test(NAME python::binding COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/python/test_fccvis.py)
  set_tests_properties(python::binding PROPERTIES LABELS needs-pybind11
                       ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:fccvis_python>")
endif()

# ---------------------------------------------------------------------------
# Build-system checks: module graph and per-module include discipline
# ---------------------------------------------------------------------------
add_subdirectory(cmake)
