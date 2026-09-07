add_library(fccvis_geometry OBJECT
    placeholder.cc
)

target_include_directories(fccvis_geometry PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(fccvis_geometry PRIVATE
    FCCVisualization::CompilerWarnings
)

set_target_properties(fccvis_geometry PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
