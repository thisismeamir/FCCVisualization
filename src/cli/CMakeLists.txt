# fccvis-shell: the interactive command shell (module api + line editing).
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/linenoise ${CMAKE_CURRENT_BINARY_DIR}/linenoise)
add_executable(fccvis-shell main.cc)
target_link_libraries(fccvis-shell PRIVATE ${FCCVIS_LINK_LIB} fccvis_linenoise FCCVisualization::CompilerWarnings)
set_target_properties(fccvis-shell PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
install(TARGETS fccvis-shell RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
