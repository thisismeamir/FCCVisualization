# Compiled documentation examples. Every code block in the Sphinx docs is
# included from one of these files (literalinclude), built by the target
# `docs-examples`, and run by CTest (label `docs`), so the docs cannot drift
# from the API.
add_custom_target(docs-examples)

# fccvis_doc_example(<name> [ARGS <arg>...] [LABELS <label>...])
function(fccvis_doc_example NAME)
  cmake_parse_arguments(E "" "" "ARGS;LABELS" ${ARGN})
  if(NOT E_LABELS)
    set(E_LABELS core)
  endif()
  add_executable(doc_${NAME} ${NAME}.cpp)
  target_link_libraries(doc_${NAME} PRIVATE ${FCCVIS_LINK_LIB} FCCVisualization::CompilerWarnings)
  set_target_properties(doc_${NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_dependencies(docs-examples doc_${NAME})
  if(FCCVIS_BUILD_TESTS)
    add_test(NAME docs::${NAME} COMMAND doc_${NAME} ${E_ARGS}
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(docs::${NAME} PROPERTIES LABELS "${E_LABELS};docs")
  endif()
endfunction()

# Library directories of the optional HEP dependencies: DD4hep loads its
# drivers and podio its data-model dictionaries through LD_LIBRARY_PATH.
set(FCCVIS_DOC_LIBPATH "")
foreach(_t DD4hep::DDCore EDM4HEP::edm4hep podio::podio ROOT::Core)
  if(TARGET ${_t})
    get_target_property(_loc ${_t} LOCATION)
    get_filename_component(_dir "${_loc}" DIRECTORY)
    list(APPEND FCCVIS_DOC_LIBPATH ${_dir})
  endif()
endforeach()
list(REMOVE_DUPLICATES FCCVIS_DOC_LIBPATH)
string(REPLACE ";" ":" FCCVIS_DOC_LIBPATH "${FCCVIS_DOC_LIBPATH}")

# fccvis_doc_script(<name> REQUIRES <option>... LABELS <label>...)
# Runs guides/<name>.fcv with fccvis-shell in a fresh directory and compares
# its output with guides/<name>.out (FCCVIS_UPDATE_GOLDEN=1 rewrites it).
# Registered only when every FCCVIS_ENABLE_* option in REQUIRES is on.
function(fccvis_doc_script NAME)
  cmake_parse_arguments(G "" "" "REQUIRES;LABELS" ${ARGN})
  if(NOT FCCVIS_BUILD_TESTS)
    return()
  endif()
  foreach(_opt ${G_REQUIRES})
    if(NOT ${_opt})
      return()
    endif()
  endforeach()
  if(NOT G_LABELS)
    set(G_LABELS core)
  endif()
  add_test(NAME docs::guide_${NAME}_shell
           COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${FCCVIS_DOC_LIBPATH}:$ENV{LD_LIBRARY_PATH}"
                   ${CMAKE_COMMAND} -DSHELL=$<TARGET_FILE:fccvis-shell>
                   -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/guides/${NAME}.fcv
                   -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/guides/${NAME}.out
                   -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                   -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/guides/${NAME}
                   -P ${PROJECT_SOURCE_DIR}/tests/shell/RunShellTest.cmake)
  set_tests_properties(docs::guide_${NAME}_shell PROPERTIES LABELS "${G_LABELS};docs")
endfunction()

fccvis_doc_example(stdlib_shapes)
fccvis_doc_example(stdlib_filters)
fccvis_doc_example(stdlib_styles)
if(FCCVIS_ENABLE_RASTER)
  fccvis_doc_example(backend_raster)
endif()
if(FCCVIS_ENABLE_CSV)
  fccvis_doc_example(adapter_csv)
endif()
if(FCCVIS_ENABLE_SVG)
  fccvis_doc_example(backend_svg)
endif()
if(FCCVIS_ENABLE_HISTOGRAM)
  fccvis_doc_example(backend_histogram)
endif()
fccvis_doc_example(api_builders)
if(FCCVIS_ENABLE_CSV AND FCCVIS_ENABLE_RASTER AND FCCVIS_BUILD_TESTS)
  add_test(NAME docs::shell_session COMMAND fccvis-shell -f ${CMAKE_CURRENT_SOURCE_DIR}/shell_session.fcv
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(docs::shell_session PROPERTIES LABELS "core;docs")
endif()
if(FCCVIS_ENABLE_CSV AND FCCVIS_BUILD_TESTS)
  add_test(NAME docs::session_config COMMAND fccvis-shell -f ${CMAKE_CURRENT_SOURCE_DIR}/session_config.json
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(docs::session_config PROPERTIES LABELS "core;docs")
endif()
if(FCCVIS_ENABLE_CSV AND FCCVIS_ENABLE_RASTER AND FCCVIS_ENABLE_SVG)
  fccvis_doc_example(quickstart ARGS ${CMAKE_CURRENT_SOURCE_DIR}/data)
  if(FCCVIS_BUILD_TESTS)
    # The quickstart script: output and picture are checked like the shell tests.
    add_test(NAME docs::quickstart_shell
             COMMAND ${CMAKE_COMMAND} -DSHELL=$<TARGET_FILE:fccvis-shell>
                     -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/quickstart.fcv
                     -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/quickstart.out
                     -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                     -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/quickstart
                     -DIMAGE=quickstart-out/event.png
                     -DGOLDEN_IMAGE=${PROJECT_SOURCE_DIR}/docs/source/_static/quickstart.png
                     -P ${PROJECT_SOURCE_DIR}/tests/shell/RunShellTest.cmake)
    set_tests_properties(docs::quickstart_shell PROPERTIES LABELS "core;docs")
  endif()
endif()
if(FCCVIS_ENABLE_CSV AND FCCVIS_ENABLE_PHOENIX AND FCCVIS_BUILD_TESTS)
  add_test(NAME docs::phoenix_session COMMAND fccvis-shell -f ${CMAKE_CURRENT_SOURCE_DIR}/phoenix_session.fcv
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(docs::phoenix_session PROPERTIES LABELS "core;docs")
endif()
if(FCCVIS_ENABLE_DD4HEP AND FCCVIS_ENABLE_RASTER AND FCCVIS_BUILD_TESTS)
  get_target_property(_ddcore DD4hep::DDCore LOCATION)
  get_filename_component(_ddlib "${_ddcore}" DIRECTORY)
  get_target_property(_rootcore ROOT::Core LOCATION)
  get_filename_component(_rootlib "${_rootcore}" DIRECTORY)
  add_test(NAME docs::detector_session
           COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${_ddlib}:${_rootlib}:$ENV{LD_LIBRARY_PATH}"
                   ${CMAKE_COMMAND} -DSHELL=$<TARGET_FILE:fccvis-shell>
                   -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/detector_session.fcv
                   -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/detector_session.out
                   -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                   -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/detector
                   -DIMAGE=detector-out/side.png
                   -DGOLDEN_IMAGE=${PROJECT_SOURCE_DIR}/docs/source/_static/detector.png
                   -P ${PROJECT_SOURCE_DIR}/tests/shell/RunShellTest.cmake)
  set_tests_properties(docs::detector_session PROPERTIES LABELS "needs-root;needs-dd4hep;docs")
endif()
if(FCCVIS_ENABLE_ROOT AND FCCVIS_ENABLE_DD4HEP AND FCCVIS_ENABLE_CSV AND FCCVIS_BUILD_TESTS)
  # Needs an X display: runs under FCCVIS_X11_LAUNCHER (xvfb-run, see tests/CMakeLists.txt).
  get_target_property(_ddcore DD4hep::DDCore LOCATION)
  get_filename_component(_ddlib "${_ddcore}" DIRECTORY)
  get_target_property(_rootcore ROOT::Core LOCATION)
  get_filename_component(_rootlib "${_rootcore}" DIRECTORY)
  add_test(NAME docs::root_session
           COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${_ddlib}:${_rootlib}:$ENV{LD_LIBRARY_PATH}"
                   ${FCCVIS_X11_LAUNCHER} $<TARGET_FILE:fccvis-shell> -f ${CMAKE_CURRENT_SOURCE_DIR}/root_session.fcv
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(docs::root_session PROPERTIES LABELS "needs-root;needs-dd4hep;needs-x11;docs")
endif()
if(FCCVIS_ENABLE_EDM4HEP AND FCCVIS_ENABLE_RASTER AND FCCVIS_BUILD_TESTS)
  set(_edmlibs "")
  foreach(_t EDM4HEP::edm4hep podio::podio ROOT::Core)
    get_target_property(_loc ${_t} LOCATION)
    get_filename_component(_dir "${_loc}" DIRECTORY)
    list(APPEND _edmlibs ${_dir})
  endforeach()
  list(REMOVE_DUPLICATES _edmlibs)
  string(REPLACE ";" ":" _edmlibs "${_edmlibs}")
  add_test(NAME docs::edm4hep_session
           COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${_edmlibs}:$ENV{LD_LIBRARY_PATH}"
                   ${CMAKE_COMMAND} -DSHELL=$<TARGET_FILE:fccvis-shell>
                   -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/edm4hep_session.fcv
                   -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/edm4hep_session.out
                   -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                   -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/edm4hep
                   -DIMAGE=edm4hep-out/event.png
                   -DGOLDEN_IMAGE=${PROJECT_SOURCE_DIR}/docs/source/_static/edm4hep.png
                   -P ${PROJECT_SOURCE_DIR}/tests/shell/RunShellTest.cmake)
  set_tests_properties(docs::edm4hep_session PROPERTIES LABELS "needs-root;needs-podio;needs-edm4hep;docs")
endif()

# ---------------------------------------------------------------------------
# Guides (docs/source/guides): shell scripts with checked output, C++ examples.
# ---------------------------------------------------------------------------
fccvis_doc_script(detector REQUIRES FCCVIS_ENABLE_DD4HEP FCCVIS_ENABLE_RASTER LABELS needs-root needs-dd4hep)
fccvis_doc_script(edm4hep REQUIRES FCCVIS_ENABLE_EDM4HEP FCCVIS_ENABLE_HISTOGRAM LABELS needs-root needs-podio needs-edm4hep)
fccvis_doc_script(csv REQUIRES FCCVIS_ENABLE_CSV)
fccvis_doc_script(filters REQUIRES FCCVIS_ENABLE_CSV FCCVIS_ENABLE_HISTOGRAM)
fccvis_doc_script(styling REQUIRES FCCVIS_ENABLE_CSV FCCVIS_ENABLE_SVG)
fccvis_doc_script(layouts REQUIRES FCCVIS_ENABLE_CSV FCCVIS_ENABLE_RASTER)
fccvis_doc_script(shell REQUIRES FCCVIS_ENABLE_CSV FCCVIS_ENABLE_RASTER)
fccvis_doc_script(phoenix_json REQUIRES FCCVIS_ENABLE_PHOENIX_JSON FCCVIS_ENABLE_SVG)
fccvis_doc_script(multi_backend REQUIRES FCCVIS_ENABLE_CSV FCCVIS_ENABLE_RASTER FCCVIS_ENABLE_SVG FCCVIS_ENABLE_HISTOGRAM)
if(FCCVIS_ENABLE_CSV)
  fccvis_doc_example(guide_filters ARGS ${CMAKE_CURRENT_SOURCE_DIR}/data)
  fccvis_doc_example(guide_scripting ARGS ${CMAKE_CURRENT_SOURCE_DIR}/data)
endif()
if(FCCVIS_ENABLE_CSV AND FCCVIS_ENABLE_RASTER AND FCCVIS_ENABLE_SVG AND FCCVIS_ENABLE_HISTOGRAM)
  fccvis_doc_example(guide_multi_backend ARGS ${CMAKE_CURRENT_SOURCE_DIR}/data)
endif()

# Extending (docs/source/development/index.md)
fccvis_doc_example(extend_adapter ARGS ${CMAKE_CURRENT_SOURCE_DIR}/data)
fccvis_doc_example(extend_backend)

# Python binding (docs/source/api/python.md)
if(FCCVIS_ENABLE_PYTHON AND FCCVIS_ENABLE_CSV AND FCCVIS_ENABLE_RASTER AND FCCVIS_BUILD_TESTS)
  add_test(NAME docs::python COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/python_quickstart.py
                                     ${CMAKE_CURRENT_SOURCE_DIR}/data
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(docs::python PROPERTIES LABELS "needs-pybind11;docs"
                       ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:fccvis_python>")
endif()

if(FCCVIS_ENABLE_REVE AND FCCVIS_ENABLE_CSV AND FCCVIS_BUILD_TESTS)
  add_test(NAME docs::reve_session COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${FCCVIS_DOC_LIBPATH}:$ENV{LD_LIBRARY_PATH}"
                   $<TARGET_FILE:fccvis-shell> -f ${CMAKE_CURRENT_SOURCE_DIR}/reve_session.fcv
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(docs::reve_session PROPERTIES LABELS "needs-root;docs" PASS_REGULAR_EXPRESSION "REve page for 'web': http://")
endif()

if(FCCVIS_ENABLE_CED AND FCCVIS_ENABLE_CSV AND FCCVIS_BUILD_TESTS)
  add_test(NAME docs::ced_session COMMAND fccvis-shell -f ${CMAKE_CURRENT_SOURCE_DIR}/ced_session.fcv
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(docs::ced_session PROPERTIES LABELS "core;docs")
endif()
