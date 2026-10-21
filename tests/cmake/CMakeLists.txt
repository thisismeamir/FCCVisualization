# Build-system tests. The scratch projects under module_graph/ configure a
# handful of fake modules with fccvis_add_module and must fail (cycle,
# disallowed edge, third-party link in core) or pass (valid graph).

set(_mg ${CMAKE_CURRENT_SOURCE_DIR}/module_graph)
set(_mgb ${CMAKE_CURRENT_BINARY_DIR}/module_graph)

function(_fccvis_graph_test NAME CASE REGEX)
  add_test(NAME build::module_graph_${NAME}
    COMMAND ${CMAKE_COMMAND} -S ${_mg} -B ${_mgb}/${NAME}
            -DFCCVIS_CMAKE_DIR=${PROJECT_SOURCE_DIR}/cmake -DCASE=${CASE})
  set_tests_properties(build::module_graph_${NAME} PROPERTIES
    PASS_REGULAR_EXPRESSION "${REGEX}" LABELS build)
endfunction()

_fccvis_graph_test(cycle      cycle      "module dependency cycle: [a-z_]+ -> [a-z_]+ -> [a-z_]+")
_fccvis_graph_test(disallowed disallowed "may not depend on 'scene_x'")
_fccvis_graph_test(third_party third_party "may not link third-party targets")
_fccvis_graph_test(valid      valid      "module graph checked \\(3 modules")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_test(NAME build::module_includes
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_module_includes.py
            ${CMAKE_BINARY_DIR}/fccvis_module_graph.json)
  set_tests_properties(build::module_includes PROPERTIES LABELS build)
  add_test(NAME build::labels
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_test_labels.py
            ${CMAKE_CTEST_COMMAND} ${CMAKE_BINARY_DIR})
  set_tests_properties(build::labels PROPERTIES LABELS build)
  find_package(Git QUIET)
  if(GIT_FOUND AND EXISTS ${PROJECT_SOURCE_DIR}/.git)
    add_test(NAME build::license_headers COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_license_headers.py)
    set_tests_properties(build::license_headers PROPERTIES LABELS build)
  endif()
endif()
