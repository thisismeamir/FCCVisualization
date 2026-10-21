# FCCVisOptions.cmake
#
#   fccvis_dependent_option(<VAR> <doc> <available> <reason-if-missing>)
#
# Declares FCCVIS_ENABLE_<NAME>. Its default is <available>. Forcing it ON when
# the dependency is missing fails the configure step with <reason-if-missing>,
# so a disabled adapter or backend is never half-built. Disabled means the
# module is neither compiled nor declared.

function(fccvis_dependent_option VAR DOC AVAILABLE REASON)
  option(${VAR} "${DOC}" ${AVAILABLE})
  if(${VAR} AND NOT ${AVAILABLE})
    message(FATAL_ERROR "${VAR}=ON but its dependency is missing: ${REASON}")
  endif()
  set_property(GLOBAL APPEND PROPERTY FCCVIS_ENABLE_OPTIONS ${VAR})
endfunction()

function(fccvis_print_options)
  get_property(opts GLOBAL PROPERTY FCCVIS_ENABLE_OPTIONS)
  list(SORT opts)
  foreach(o ${opts})
    if(${o})
      message(STATUS "fccvis: ${o} = ON")
    else()
      message(STATUS "fccvis: ${o} = OFF")
    endif()
  endforeach()
endfunction()
