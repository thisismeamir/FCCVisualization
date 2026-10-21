# Vendored linenoise (BSD-2-Clause), see README.fccvis.md. Used by fccvis-shell only.
enable_language(C)
add_library(fccvis_linenoise STATIC linenoise.c)
target_include_directories(fccvis_linenoise PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(fccvis_linenoise PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON
                      COMPILE_WARNING_AS_ERROR OFF)
target_compile_options(fccvis_linenoise PRIVATE -w)
