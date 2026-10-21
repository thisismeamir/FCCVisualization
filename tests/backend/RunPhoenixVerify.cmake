# Writes Phoenix output with fccvis-shell and loads it into the Phoenix event display.
#   cmake -DSHELL=... -DSCRIPT=... -DWORKDIR=... -DNODE=... -DVERIFY=... -P RunPhoenixVerify.cmake
file(REMOVE_RECURSE ${WORKDIR})
file(MAKE_DIRECTORY ${WORKDIR})
execute_process(COMMAND ${SHELL} -f ${SCRIPT} WORKING_DIRECTORY ${WORKDIR} RESULT_VARIABLE rc OUTPUT_QUIET)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "fccvis-shell failed (${rc})")
endif()
execute_process(COMMAND ${NODE} ${VERIFY} ${WORKDIR}/phoenix-out ${WORKDIR}/phoenix.png
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}${err}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "Phoenix check failed")
endif()
