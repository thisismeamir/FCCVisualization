add_library(fccvis_changes OBJECT
    placeholder.cc
)

target_include_directories(fccvis_changes PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_changes PRIVATE
    FCCVisualization::CompilerWarnings
    fccvis_filter
    fccvis_data
    fccvis_scene
  )

set_target_properties(fccvis_changes PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
