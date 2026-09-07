add_executable(fccvis
    main.cpp
    CliOptions.cc
    RunApplication.cc
)

target_include_directories(fccvis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis PRIVATE
    ${FCCVIS_LINK_LIB}
    FCCVisualization::CompilerWarnings
)

set_target_properties(fccvis PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

include(GNUInstallDirs)
install(TARGETS fccvis RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
