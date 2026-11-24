# Python binding (T5.8): module `fccvis`, built with pybind11 against the shared library.
pybind11_add_module(fccvis_python MODULE module.cc)
set_target_properties(fccvis_python PROPERTIES OUTPUT_NAME fccvis CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
                      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/python)
target_link_libraries(fccvis_python PRIVATE ${FCCVIS_LINK_LIB})
# The registries are function-local statics of inline functions in headers; with
# pybind11's default hidden visibility the module would get its own, empty copies
# instead of the library's (where the adapters and backends registered).
set_target_properties(fccvis_python PROPERTIES CXX_VISIBILITY_PRESET default VISIBILITY_INLINES_HIDDEN OFF)

include(GNUInstallDirs)
install(TARGETS fccvis_python LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/fccvis/python COMPONENT Python)
