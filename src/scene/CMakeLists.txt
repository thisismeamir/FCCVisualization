add_library(fccvis_scene OBJECT
    placeholder.cc
)

target_include_directories(fccvis_scene PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_scene PRIVATE
    FCCVisualization::CompilerWarnings
)

set_target_properties(fccvis_scene PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
