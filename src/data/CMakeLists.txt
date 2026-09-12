add_library(fccvis_data OBJECT
    data.cc
)

target_include_directories(fccvis_data PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_data PRIVATE
    FCCVisualization::CompilerWarnings
)

set_target_properties(fccvis_data PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
