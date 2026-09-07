add_library(fccvis_filter OBJECT
    placeholder.cc
)

target_include_directories(fccvis_filter PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_filter PRIVATE
    FCCVisualization::CompilerWarnings
)

set_target_properties(fccvis_filter PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
