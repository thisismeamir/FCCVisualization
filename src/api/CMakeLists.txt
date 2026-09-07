add_library(fccvis_api OBJECT
    placeholder.cc
)

target_include_directories(fccvis_api PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_api PRIVATE
    FCCVisualization::CompilerWarnings
)

set_target_properties(fccvis_api PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
