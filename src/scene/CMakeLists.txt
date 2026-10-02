add_library(fccvis_scene OBJECT
    placeholder.cc
)

target_include_directories(fccvis_scene PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_scene PRIVATE
    FCCVisualization::CompilerWarnings
    fccvis_filter
    fccvis_geometry
  )

set_target_properties(fccvis_scene PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
