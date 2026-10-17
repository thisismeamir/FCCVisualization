# Documentation: Doxygen XML (API coverage: undocumented public symbols fail),
# generated reference pages, and the Sphinx site (warnings are errors).
#   cmake --build <build> --target docs        # site in <build>/docs/html
# Requires Doxygen and sphinx-build with the packages in docs/requirements.txt.
find_package(Doxygen QUIET)
find_program(FCCVIS_SPHINX_BUILD NAMES sphinx-build)
if(NOT DOXYGEN_FOUND OR NOT FCCVIS_SPHINX_BUILD)
  message(WARNING "FCCVIS_BUILD_DOCS: Doxygen or sphinx-build not found; the docs target is not available")
  return()
endif()

# Public headers: the include directories of the library modules (adapter and
# backend implementations are private; users reach them through registries).
set(_inputs "")
get_property(_mods GLOBAL PROPERTY FCCVIS_LIBRARY_MODULES)
foreach(_m ${_mods})
  get_property(_dir GLOBAL PROPERTY FCCVIS_MODULE_${_m}_DIR)
  if(EXISTS ${_dir}/include)
    list(APPEND _inputs ${_dir}/include)
  endif()
endforeach()
list(APPEND _inputs ${FCCVIS_GENERATED_INCLUDE_DIR}/fccvis/FccvisConfig.h)
list(JOIN _inputs " " FCCVIS_DOXYGEN_INPUT)
set(FCCVIS_DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
set(FCCVIS_DOXYGEN_STRIP ${PROJECT_SOURCE_DIR}/src)
set(FCCVIS_DOXYGEN_WARN_AS_ERROR FAIL_ON_WARNINGS)
configure_file(Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

add_custom_target(docs-doxygen
  COMMAND ${CMAKE_COMMAND} -E make_directory ${FCCVIS_DOXYGEN_OUT}
  COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMENT "Doxygen XML (undocumented public symbols fail the build)" VERBATIM)

# Reference pages generated from the command table and the registries.
set(_gen ${CMAKE_CURRENT_SOURCE_DIR}/source/reference/generated)
add_custom_target(docs-reference
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_gen}
  COMMAND fccvis-shell --reference commands > ${_gen}/commands.md
  COMMAND fccvis-shell --reference adapters > ${_gen}/adapters.md
  COMMAND fccvis-shell --reference backends > ${_gen}/backends.md
  DEPENDS fccvis-shell
  COMMENT "Generated reference pages" VERBATIM)

add_custom_target(docs
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/doctrees
  COMMAND ${CMAKE_COMMAND} -E env FCCVIS_DOXYGEN_XML=${FCCVIS_DOXYGEN_OUT}/xml FCCVIS_VERSION=${PROJECT_VERSION}
          ${FCCVIS_SPHINX_BUILD} -W --keep-going -q -b html
          ${CMAKE_CURRENT_SOURCE_DIR}/source ${CMAKE_CURRENT_BINARY_DIR}/html
          -d ${CMAKE_CURRENT_BINARY_DIR}/doctrees
  DEPENDS docs-doxygen docs-reference docs-examples
  COMMENT "Sphinx site (warnings are errors)" VERBATIM)
