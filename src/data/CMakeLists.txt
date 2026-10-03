add_library(fccvis_data OBJECT
    placeholder.cc
)

target_include_directories(fccvis_data PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_data PRIVATE
    FCCVisualization::CompilerWarnings
    fccvis_geometry
  )

set_target_properties(fccvis_data PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
