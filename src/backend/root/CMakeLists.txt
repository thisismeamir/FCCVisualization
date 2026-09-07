add_library(fccvis_backend_root OBJECT
  placeholder.cc
)

target_include_directories(fccvis_backend_root PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_backend_root PRIVATE
  FCCVisualization::CompilerWarnings
)

set_target_properties(fccvis_backend_root PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  POSITION_INDEPENDENT_CODE ON
)
