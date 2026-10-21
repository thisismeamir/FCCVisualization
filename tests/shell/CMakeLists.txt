# Shell tests: scripted sessions compared with expected output, and an
# interactive test on a pseudo-terminal (needs Python 3).
if(NOT (FCCVIS_ENABLE_CSV AND FCCVIS_ENABLE_RASTER))
  return()
endif()
add_test(NAME shell::session
         COMMAND ${CMAKE_COMMAND} -DSHELL=$<TARGET_FILE:fccvis-shell>
                 -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/session.fcv
                 -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/session.out
                 -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                 -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/session
                 -DARGS=--keep-going
                 -DIMAGE=shell-out/layout.png
                 -DGOLDEN_IMAGE=${PROJECT_SOURCE_DIR}/tests/data/golden/shell_layout.png
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/RunShellTest.cmake)
set_tests_properties(shell::session PROPERTIES LABELS core)

add_test(NAME shell::reload
         COMMAND ${CMAKE_COMMAND} -DSHELL=$<TARGET_FILE:fccvis-shell>
                 -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/reload.fcv
                 -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/reload.out
                 -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                 -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/reload
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/RunShellTest.cmake)
set_tests_properties(shell::reload PROPERTIES LABELS core)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_test(NAME shell::interactive
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_interactive.py $<TARGET_FILE:fccvis-shell>
                   ${CMAKE_CURRENT_BINARY_DIR}/interactive)
  set_tests_properties(shell::interactive PROPERTIES LABELS core TIMEOUT 60)
endif()
