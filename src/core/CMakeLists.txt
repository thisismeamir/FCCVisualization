add_library(fccvis_core_impl OBJECT
    placeholder.cc
)

target_include_directories(fccvis_core_impl PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_core_impl PRIVATE
    FCCVisualization::CompilerWarnings
)

set_target_properties(fccvis_core_impl PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
