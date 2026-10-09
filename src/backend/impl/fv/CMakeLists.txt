add_library(fccvis_backend_fv OBJECT
    FVBackend.cc
)

target_include_directories(fccvis_backend_fv
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}
    
)

target_link_libraries(fccvis_backend_fv PRIVATE
    FCCVisualization::CompilerWarnings
    fccvis_geometry
    fccvis_filter
    fccvis_data
    fccvis_changes
    fccvis_scene
)

set_target_properties(fccvis_backend_fv PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
