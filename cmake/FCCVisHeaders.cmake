# Symlinks a module's public headers into <build>/include/fccvis/ so that
# out-of-tree code can use one include directory.

function(fccvis_export_headers_from SOURCE_DIR)
  set(DEST_DIR "${CMAKE_BINARY_DIR}/include/fccvis")
  if(NOT EXISTS "${SOURCE_DIR}")
    return()
  endif()

  file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "${SOURCE_DIR}/*")

  foreach(HEADER ${HEADERS})
    if(IS_DIRECTORY "${HEADER}")
      continue()
    endif()
    file(RELATIVE_PATH RELATIVE "${SOURCE_DIR}" "${HEADER}")
    set(DESTINATION "${DEST_DIR}/${RELATIVE}")
    get_filename_component(DEST_PARENT "${DESTINATION}" DIRECTORY)
    file(MAKE_DIRECTORY "${DEST_PARENT}")
    file(CREATE_LINK "${HEADER}" "${DESTINATION}" SYMBOLIC)
  endforeach()
endfunction()

# Backward-compatible name: exports src/<COMPONENT>/include.
function(fccvis_export_headers COMPONENT)
  fccvis_export_headers_from("${CMAKE_SOURCE_DIR}/src/${COMPONENT}/include")
endfunction()
