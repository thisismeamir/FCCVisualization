add_library(fccvis_geometry OBJECT
    Geometry.cc
    Adapters.cc
  )

target_include_directories(fccvis_geometry PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(ROOT REQUIRED COMPONENTS Core Geom)
find_package(DD4hep REQUIRED COMPONENTS DDCore)

target_link_libraries(fccvis_geometry PUBLIC
  ROOT::Core
  ROOT::Geom
  DD4hep::DDCore
)


target_link_libraries(fccvis_geometry PRIVATE
    FCCVisualization::CompilerWarnings
)

set_target_properties(fccvis_geometry PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
