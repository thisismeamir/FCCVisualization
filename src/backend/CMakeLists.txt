option(BUILD_BACKEND_FV "Build the FV backend" ON)

add_subdirectory(impl)

add_library(fccvis_backend OBJECT
    placeholder.cc
)

target_include_directories(fccvis_backend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_backend PRIVATE
    FCCVisualization::CompilerWarnings
    fccvis_data
    fccvis_changes
    fccvis_scene
    ${ENABLED_BACKEND_TARGETS}
)

set_target_properties(fccvis_backend PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
