add_library(fccvis_visualization OBJECT
    placeholder.cc
)

target_include_directories(fccvis_visualization PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_visualization PRIVATE
    FCCVisualization::CompilerWarnings
)

set_target_properties(fccvis_visualization PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
