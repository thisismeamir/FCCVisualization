# Runs fccvis-shell on a script and compares its output with an expected file.
#   cmake -DSHELL=... -DSCRIPT=... -DEXPECTED=... -DSOURCE_DIR=... -DWORKDIR=...
#         [-DARGS=...] [-DIMAGE=<produced png> -DGOLDEN_IMAGE=<png>] -P RunShellTest.cmake
# FCCVIS_UPDATE_GOLDEN=1 rewrites the expected files.
file(REMOVE_RECURSE ${WORKDIR})
file(MAKE_DIRECTORY ${WORKDIR})
separate_arguments(_args UNIX_COMMAND "${ARGS}")
execute_process(COMMAND ${SHELL} ${_args} -f ${SCRIPT}
                WORKING_DIRECTORY ${WORKDIR}
                OUTPUT_VARIABLE _out ERROR_VARIABLE _err RESULT_VARIABLE _rc)
set(_actual "${_out}--- stderr\n${_err}--- exit status ${_rc}\n")
string(REPLACE "${SOURCE_DIR}" "<src>" _actual "${_actual}")
file(WRITE ${WORKDIR}/actual.out "${_actual}")
if(DEFINED ENV{FCCVIS_UPDATE_GOLDEN} AND NOT "$ENV{FCCVIS_UPDATE_GOLDEN}" STREQUAL "0")
  file(WRITE ${EXPECTED} "${_actual}")
  if(IMAGE)
    file(COPY_FILE ${WORKDIR}/${IMAGE} ${GOLDEN_IMAGE})
  endif()
  message(STATUS "updated ${EXPECTED}")
  return()
endif()
file(READ ${EXPECTED} _expected)
if(NOT _actual STREQUAL _expected)
  execute_process(COMMAND diff -u ${EXPECTED} ${WORKDIR}/actual.out OUTPUT_VARIABLE _diff)
  message(FATAL_ERROR "shell output differs from ${EXPECTED}:\n${_diff}\nrerun with FCCVIS_UPDATE_GOLDEN=1 if intended")
endif()
if(IMAGE)
  file(SHA256 ${WORKDIR}/${IMAGE} _a)
  file(SHA256 ${GOLDEN_IMAGE} _b)
  if(NOT _a STREQUAL _b)
    message(FATAL_ERROR "${WORKDIR}/${IMAGE} differs from ${GOLDEN_IMAGE}")
  endif()
endif()
