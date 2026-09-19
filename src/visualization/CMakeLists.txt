add_library(fccvis_visualization OBJECT
  CollectionVisualizer.cc
)

target_include_directories(fccvis_visualization PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(podio CONFIG REQUIRED)
find_package(EDM4HEP CONFIG REQUIRED)

target_link_libraries(fccvis_visualization PUBLIC
    FCCVisualization::CompilerWarnings
    podio::podio
    podio::podioRootIO
    EDM4HEP::edm4hep

  )

set_target_properties(fccvis_visualization PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
